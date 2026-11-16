# Паралельна пошукова система з LLM

Пошуковий сервер на C++20. Будує інвертований індекс над текстовими файлами кількома потоками (власний пул потоків і власна паралельна хеш-таблиця) і шукає за ключовими словами з ранжуванням TF-IDF. Запит можна розширити синонімами через LLM (Ollama, модель `qwen2.5:0.5b`). API доступне через REST і WebSocket. До сервера є два клієнти: CLI на C++ і Web UI.

## Зміст

- [Що потрібно](#що-потрібно)
- [Запуск](#запуск)
- [Датасет](#датасет)
- [Як працює пошук](#як-працює-пошук)
- [CLI](#cli)
- [Web UI](#web-ui)
- [API](#api)
- [Змінні середовища](#змінні-середовища)
- [Структура репозиторію](#структура-репозиторію)
- [Збірка без Docker](#збірка-без-docker)

## Що потрібно

- Docker з Docker Compose v2 (команда `docker compose`).
- Вільний порт `8080`.
- Інтернет під час першого запуску: збирається образ сервера і завантажується модель Ollama (близько 400 МБ).

На хості більше нічого встановлювати не потрібно: компілятор, бібліотеки й CLI-клієнт є в образі.

## Запуск

```bash
git clone <адреса репозиторію>
cd <папка репозиторію>
docker compose up
```

Compose піднімає три сервіси:

| Сервіс | Що робить |
|---|---|
| `search-server` | пошуковий сервер, порт `8080` |
| `ollama` | LLM-сервер Ollama, доступний лише всередині мережі Compose |
| `ollama-init` | одноразово виконує `ollama pull` для моделі й завершується |

Модель зберігається в томі `ollama_models`, тому наступні запуски її не завантажують. Поки модель завантажується, сервер уже працює: `GET /api/health` повертає `"ollama": false`, а пошук з розширенням виконується без нього (`expanded_terms` порожній).

Після запуску індекс порожній. Його треба побудувати, потім можна шукати:

```bash
./search index --threads 4
./search "storage battery"
```

Зупинка: `docker compose down`. Команда `docker compose down -v` також видаляє том із моделлю.

## Датасет

**Де лежить.** За замовчуванням сервер індексує папку `data/` у корені репозиторію (у Git вона порожня). Вона монтується в контейнер як `/data` лише для читання. Іншу папку можна вказати змінною `DATASET_DIR`:

```bash
DATASET_DIR=/home/user/books docker compose up
```

У PowerShell:

```powershell
$env:DATASET_DIR = "D:\books"; docker compose up
```

У cmd:

```bat
set DATASET_DIR=D:\books
docker compose up
```

Замість цього можна скопіювати `.env.example` у `.env` і вказати шлях там.

**Формат.**

- Тексти англійською мовою у файлах з розширенням `.txt`. Один файл є одним документом.
- Файли шукаються рекурсивно в усіх підпапках. Файли з іншими розширеннями (зокрема `.TXT`) ігноруються.
- `doc_id` документа дорівнює імені файлу без `.txt`: `data/test/1001.txt` має `doc_id` `"1001"`.
- Кодування: ASCII або UTF-8. Індексуються лише латинські літери й цифри (див. [Як працює пошук](#як-працює-пошук)).
- Порожня папка не є помилкою: індекс матиме 0 документів.

**Обмеження.**

- Файли з однаковими іменами в різних підпапках отримають однаковий `doc_id` у відповідях.
- Датасет не має змінюватися, поки сервер працює. Якщо файли змінилися, індекс треба перебудувати (`POST /api/index`).

Під час розробки використовувався датасет PG-19 (книги Project Gutenberg, виданих до 1919 року): файли `.txt` з нього можна просто скопіювати в `data/`.

## Як працює пошук

**Нормалізація тексту** однакова для індексації і для запиту:

1. Текст переводиться в нижній регістр.
2. Термін це послідовність латинських літер і цифр `[a-z0-9]+`. Усе інше (пробіли, розділові знаки, апостроф, не-ASCII символи) розділяє терміни: `don't` дає `don` і `t`.
3. Кожен термін проходить стемінг алгоритмом Портера: `batteries`, `battery` → `batteri`. Слова до 2 символів не змінюються.
4. Стоп-слова (`the`, `and`, ...) не видаляються.

**Що з цього випливає:**

- Шукаються цілі слова після стемінгу. Підрядки не шукаються: `whal` не знайде `whale`.
- Кирилиця та інші не-латинські тексти не індексуються. Запит без латинських літер і цифр повертає порожній список результатів (не помилку).
- Документ потрапляє у видачу, якщо містить хоча б один термін запиту.

**Ранжування.** Для документа `d` і термінів запиту `t`:

```
score(d) = сума по t: weight(t) * (1 + log10 tf(t, d)) * log10(N / df(t))
```

де `tf` кількість входжень терміна в документ, `N` кількість документів в індексі, `df` кількість документів із терміном. `weight` дорівнює 1 для слів запиту і 0,5 для слів, доданих LLM. Якщо слово є і в запиті, і в розширенні, воно рахується один раз з вагою 1. Результати сортуються за `score` за спаданням, при рівному `score` вище документ з меншим внутрішнім номером.

**Snippet.** Для кожного результату сервер відкриває файл і бере близько 200 символів навколо першого входження терміна, який дав документу найбільший внесок у `score`. Межі зсуваються до пробілу, обрізані краї позначаються `...`. Якщо терміна в тексті не знайдено, береться початок файлу; якщо файл не читається, snippet порожній.

**Розширення запиту (LLM).** У режимі `expanded` (і завжди у WebSocket) запит надсилається в Ollama `/api/generate`, і модель повертає до 6 пов'язаних слів. Запити до Ollama йдуть по одному через чергу. Якщо Ollama недоступна, відповідь некоректна або очікування в черзі з викликом перевищило `LLM_TIMEOUT_SEC` (30 с), пошук виконується за оригінальним запитом, а `expanded_terms` порожній.

## CLI

CLI-клієнт написаний на C++ і зібраний в образі `search-server`, тому запускається через `docker compose exec`. Сервіси мають бути запущені (`docker compose up`). Команди виконуються з кореня репозиторію.

| Оболонка | Команда |
|---|---|
| Linux, macOS, Git Bash, WSL | `./search "запит"` |
| Windows cmd | `search "запит"` (скрипт `search.cmd`) |
| Windows PowerShell | `.\search.cmd "запит"` (PowerShell не запускає скрипти з поточної папки без шляху) |
| Будь-яка | `docker compose exec search-server search "запит"` |

Скрипти `search` і `search.cmd` лише викликають третю команду.

**Команди:**

```
search <слова запиту...> [--mode keyword|expanded] [--limit N]
search index [--threads N]
search stats
search health
```

| Команда | REST-запит | Опис |
|---|---|---|
| `search <слова>` | `POST /api/search` | пошук; слова можна писати в лапках або без них |
| `--mode` | | `keyword` (за замовчуванням) або `expanded` (з розширенням через LLM) |
| `--limit N` | | кількість результатів, 1..100, за замовчуванням 10 |
| `index [--threads N]` | `POST /api/index` | побудувати індекс N потоками (1..256; без опції кількість ядер) |
| `stats` | `GET /api/stats` | статистика останньої побудови |
| `health` | `GET /api/health` | стан сервера й Ollama |

Щоб шукати слова `index`, `stats` або `health`, перед запитом пишеться `--`: `./search -- index`.

**Приклад:**

```
$ ./search storage battery --limit 2
Query: storage battery
Found 2 results in 1 ms

1. 820  (score 5.656)
   ...PORTLAND CEMENT XXI. MOTION PICTURES XXII. THE DEVELOPMENT OF THE EDISON STORAGE BATTERY XXIII. MISCELLANEOUS INVENTIONS XXIV. EDISON'S METHOD IN INVENTING ...

2. 950  (score 4.499)
   ...an electric runabout, dad." "An electric runabout? But it will have to be operated with a storage battery, Tom, and you haven't--" "I guess you're going to say I haven't any storage battery,...
```

У режимі `expanded` перед результатами виводиться рядок `Expanded terms: ...`.

**Коди виходу:** `0` успіх, `1` помилка (сервер повернув помилку або неправильні аргументи), `2` немає з'єднання із сервером.

**Адреса сервера:** змінна `SEARCH_URL`, за замовчуванням `http://localhost:$HTTP_PORT` (у контейнері `http://localhost:8080`).

## Web UI

Web UI відкривається за адресою **http://localhost:8080** після `docker compose up`. Це одна статична сторінка, яку віддає сам `search-server`.

- **Пошук** іде через WebSocket `/ws/search`: етапи пайплайну (`query_expansion` → `keyword_search` → `done`) показуються в міру надходження, далі слова від LLM і результати з підсвіченими словами запиту в snippet.
- **Журнал WebSocket** показує надіслані й отримані повідомлення.
- **Керування** через REST: стан сервера, Ollama і WebSocket (`GET /api/health`), побудова індексу з вибором кількості потоків (`POST /api/index`), статистика (`GET /api/stats`). Для кожного запиту видно код відповіді, поля відповіді та повний JSON.

WebSocket завжди повертає 10 результатів; параметр `limit` є лише в REST і CLI.

## API

Сервер слухає порт `8080`. Тіла запитів і відповідей REST у форматі JSON. Відповіді мають заголовок `Content-Type: application/json`. Невідомі поля в тілі запиту ігноруються.

### Помилки

Будь-яка помилка REST має однаковий формат:

```json
{"status": "error", "message": "index is not ready"}
```

| Поле | Тип | Опис |
|---|---|---|
| `status` | string | завжди `"error"` |
| `message` | string | опис помилки |

| Код | Коли |
|---|---|
| `400` | тіло не є JSON-об'єктом або поле має неправильний тип чи значення |
| `404` | невідомий шлях (`"unknown path"`) |
| `405` | шлях існує, але для іншого методу (`"method not allowed"`) |
| `409` | індекс ще не побудовано або індексація вже триває |
| `500` | внутрішня помилка сервера |

### Огляд

| Метод | Шлях | Опис |
|---|---|---|
| `GET` | `/api/health` | стан сервера й Ollama |
| `POST` | `/api/index` | побудувати (перебудувати) індекс |
| `GET` | `/api/stats` | статистика індексу |
| `POST` | `/api/search` | пошук |
| WebSocket | `/ws/search` | пошук з передачею етапів пайплайну |
| `GET` | `/` | сторінка Web UI (HTML) |

### `GET /api/health`

Тіла запиту немає. Завжди повертає `200`, навіть коли Ollama недоступна.

```json
{"status": "ok", "ollama": true}
```

| Поле | Тип | Опис |
|---|---|---|
| `status` | string | завжди `"ok"` |
| `ollama` | boolean | `true`, якщо Ollama відповідає і модель `OLLAMA_MODEL` уже завантажена; інакше `false` |

### `POST /api/index`

Будує інвертований індекс над усіма файлами `.txt` датасету заданою кількістю потоків і відповідає після завершення побудови. Повторний виклик перебудовує індекс з нуля. Поки індекс будується, пошук повертає `409`.

Запит:

```json
{"threads": 4}
```

| Поле | Тип | Обов'язкове | Опис |
|---|---|---|---|
| `threads` | integer | ні | кількість потоків, 1..256; без поля (або з порожнім тілом) кількість логічних ядер |

Відповідь `200`:

```json
{"status": "ok", "documents_indexed": 1436, "time_ms": 4816}
```

| Поле | Тип | Опис |
|---|---|---|
| `status` | string | `"ok"` |
| `documents_indexed` | integer | кількість проіндексованих документів |
| `time_ms` | integer | час побудови в мілісекундах |

Помилки:

| Код | `message` |
|---|---|
| `400` | `request body must be a JSON object` |
| `400` | `threads must be an integer` |
| `400` | `threads must be between 1 and 256` |
| `409` | `indexing is already in progress` |
| `500` | `indexing failed: ...` |

### `GET /api/stats`

Тіла запиту немає. До першої побудови індексу всі поля дорівнюють `0`.

```json
{"documents": 1436, "index_build_time_ms": 4816, "threads_used": 8}
```

| Поле | Тип | Опис |
|---|---|---|
| `documents` | integer | кількість документів в індексі |
| `index_build_time_ms` | integer | час останньої побудови в мілісекундах |
| `threads_used` | integer | кількість потоків останньої побудови |

### `POST /api/search`

Запит:

```json
{"query": "storage battery", "mode": "keyword", "limit": 2}
```

| Поле | Тип | Обов'язкове | Опис |
|---|---|---|---|
| `query` | string | так | текст запиту, непорожній |
| `mode` | string | ні | `"keyword"` (за замовчуванням): лише пошук за індексом, LLM не викликається; `"expanded"`: спершу розширення запиту через LLM, потім пошук |
| `limit` | integer | ні | максимальна кількість результатів, 1..100, за замовчуванням 10 |

Відповідь `200` у режимі `keyword`:

```json
{
  "results": [
    {
      "doc_id": "820",
      "score": 5.655675604423009,
      "snippet": "...PORTLAND CEMENT XXI. MOTION PICTURES XXII. THE DEVELOPMENT OF THE EDISON STORAGE BATTERY XXIII. MISCELLANEOUS INVENTIONS XXIV. EDISON'S METHOD IN INVENTING ..."
    },
    {
      "doc_id": "950",
      "score": 4.499057027770006,
      "snippet": "...an electric runabout, dad.\" \"An electric runabout? But it will have to be operated with a storage battery, Tom, and you haven't--\" \"I guess you're going to say I haven't any storage battery,..."
    }
  ],
  "time_ms": 2
}
```

Відповідь `200` у режимі `expanded` (запит `{"query": "whale hunting", "mode": "expanded", "limit": 1}`):

```json
{
  "expanded_terms": ["sea", "ocean", "sea life", "wildlife", "marine animals"],
  "results": [
    {
      "doc_id": "1356",
      "score": 3.8857588594001595,
      "snippet": "...by An Anonymous Volunteer THE CRUISE OF THE \"CACHALOT\" ROUND THE WORLD AFTER SPERM WHALES Frank T. Bullen, F.R.G.S. First Mate To Miss Emily Hensley In..."
    }
  ],
  "time_ms": 1611
}
```

| Поле | Тип | Опис |
|---|---|---|
| `results` | array of object | результати, відсортовані за `score` за спаданням; порожній масив, якщо нічого не знайдено |
| `results[].doc_id` | string | ім'я файлу документа без `.txt` |
| `results[].score` | number | релевантність (TF-IDF, див. [Ранжування](#як-працює-пошук)) |
| `results[].snippet` | string | фрагмент тексту навколо знайденого слова |
| `expanded_terms` | array of string | лише в режимі `expanded`: слова від LLM без стемінгу; порожній масив, якщо LLM недоступна |
| `time_ms` | integer | час обробки запиту в мілісекундах (у режимі `expanded` разом із викликом LLM) |

Слова з `expanded_terms` можуть складатися з кількох слів (`"sea life"`); при пошуку вони розбиваються на терміни так само, як запит.

Помилки:

| Код | `message` |
|---|---|
| `400` | `request body must be a JSON object` |
| `400` | `query must be a non-empty string` |
| `400` | `mode must be "keyword" or "expanded"` |
| `400` | `limit must be an integer` |
| `400` | `limit must be between 1 and 100` |
| `409` | `index is not ready` (індекс ще не побудовано або він перебудовується) |
| `500` | `search failed: ...` |

### WebSocket `/ws/search`

Адреса: `ws://localhost:8080/ws/search`. Усі повідомлення текстові, у форматі JSON.

**Клієнт → сервер:**

```json
{"query": "whale hunting"}
```

| Поле | Тип | Обов'язкове | Опис |
|---|---|---|---|
| `query` | string | так | текст запиту, непорожній |

Пошук через WebSocket завжди виконується з розширенням запиту через LLM і повертає до 10 результатів.

**Сервер → клієнт.** На кожен запит сервер надсилає три повідомлення по черзі, кожне в момент завершення етапу:

1. Розширення запиту:

```json
{"stage": "query_expansion", "data": {"original": "whale hunting", "expanded_terms": ["sea", "ocean", "sea life", "wildlife", "sea creatures"]}}
```

| Поле | Тип | Опис |
|---|---|---|
| `stage` | string | `"query_expansion"` |
| `data.original` | string | запит без змін |
| `data.expanded_terms` | array of string | слова від LLM; порожній масив, якщо LLM недоступна |

2. Результати пошуку:

```json
{"stage": "keyword_search", "data": {"results": [{"doc_id": "1356", "score": 3.375746239956343, "snippet": "...by An Anonymous Volunteer THE CRUISE OF THE \"CACHALOT\" ROUND THE WORLD AFTER SPERM WHALES Frank T. Bullen, F.R.G.S. First Mate To Miss Emily Hensley In..."}], "time_ms": 24}}
```

| Поле | Тип | Опис |
|---|---|---|
| `stage` | string | `"keyword_search"` |
| `data.results` | array of object | ті самі поля, що в REST: `doc_id` (string), `score` (number), `snippet` (string) |
| `data.time_ms` | integer | час пошуку за індексом у мілісекундах (без розширення запиту) |

3. Завершення:

```json
{"stage": "done"}
```

| Поле | Тип | Опис |
|---|---|---|
| `stage` | string | `"done"` |

**Помилки** надсилаються замість етапів, з'єднання після них лишається відкритим:

```json
{"stage": "error", "data": {"message": "index is not ready"}}
```

| Поле | Тип | Опис |
|---|---|---|
| `stage` | string | `"error"` |
| `data.message` | string | опис помилки |

| `data.message` | Коли |
|---|---|
| `message must be a JSON object` | повідомлення не є JSON-об'єктом |
| `query must be a non-empty string` | немає поля `query`, воно не рядок або порожнє |
| `index is not ready` | індекс ще не побудовано або він перебудовується |
| `previous query is still in progress` | новий запит надіслано до завершення попереднього |
| `search failed: ...` | внутрішня помилка |

**Правила:**

- Одним з'єднанням можна надсилати кілька запитів, але по одному: наступний після `done` або `error`.
- Запит, надісланий раніше, відхиляється з `previous query is still in progress`, а попередній запит завершується як звичайно.

### `GET /`

Повертає сторінку Web UI (`Content-Type: text/html; charset=utf-8`). Якщо файл сторінки не знайдено, повертає `500` з `"web page is not available"`.

## Змінні середовища

Для запуску жодна змінна не потрібна: усі мають значення за замовчуванням.

**Docker Compose** (можна задати в оболонці або у файлі `.env`, див. `.env.example`):

| Змінна | За замовчуванням | Опис |
|---|---|---|
| `DATASET_DIR` | `./data` | папка з датасетом на хості, монтується в контейнер як `/data` |
| `OLLAMA_MODEL` | `qwen2.5:0.5b` | модель Ollama; `ollama-init` завантажує саме її |

**Сервер `search-server`** (у `docker-compose.yml` уже задані потрібні значення; змінювати їх треба лише при запуску без Docker):

| Змінна | За замовчуванням | У Compose | Опис |
|---|---|---|---|
| `HTTP_PORT` | `8080` | `8080` | порт HTTP і WebSocket |
| `HTTP_THREADS` | `64` | | кількість потоків обробки HTTP-запитів і запитів WebSocket |
| `OLLAMA_URL` | `http://localhost:11434` | `http://ollama:11434` | адреса Ollama |
| `OLLAMA_MODEL` | `qwen2.5:0.5b` | з Compose | модель Ollama |
| `DATA_DIR` | `./data` | `/data` | папка з датасетом |
| `WEB_DIR` | `./clients/web` | | папка зі сторінкою Web UI (`index.html`) |
| `LLM_TIMEOUT_SEC` | `30` | `30` | скільки секунд запит може чекати на LLM (у черзі й під час виклику) |

**CLI:**

| Змінна | За замовчуванням | Опис |
|---|---|---|
| `SEARCH_URL` | `http://localhost:$HTTP_PORT` | адреса сервера |

## Структура репозиторію

```
src/                  сервер
  app/                main.cpp: створює об'єкти й підключає маршрути модулів
  common/             конфігурація, JSON-відповіді, читання файлів
  concurrency/        пул потоків
  text/               токенізатор і стемер Портера
  index/              інвертований індекс: шарди, хеш-таблиця, списки постингів
  indexing/           паралельна побудова індексу, POST /api/index, GET /api/stats
  search/             пошук і ранжування, POST /api/search, WebSocket /ws/search
  llm/                клієнт Ollama і черга запитів до неї
  health/             GET /api/health
  web/                GET / (Web UI)
clients/cli/          CLI-клієнт
clients/web/          сторінка Web UI
docs/diagrams/        UML-діаграми
data/                 датасет за замовчуванням (порожній)
search, search.cmd    запуск CLI з хоста
```

Кожен модуль у `src/` має власний контролер, який реєструє свої маршрути; `src/app` лише з'єднує модулі.

## Збірка без Docker

Для налагодження сервер можна зібрати на Linux (Debian, Ubuntu) напряму:

```bash
sudo apt-get install build-essential cmake curl libasio-dev libcurl4-openssl-dev nlohmann-json3-dev gdb
sudo curl -fL -o /usr/local/include/crow_all.h https://github.com/CrowCpp/Crow/releases/download/v1.3.4/crow_all.h
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Запуск з кореня репозиторію (тоді `DATA_DIR` і `WEB_DIR` за замовчуванням вказують на `./data` і `./clients/web`):

```bash
./build/search_server
./build/search "storage battery"
```

Без Ollama на `localhost:11434` сервер працює, лише розширення запиту повертає порожній список. Налагодження: `gdb ./build/search_server`.
