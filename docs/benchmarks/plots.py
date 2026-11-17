"""Builds the charts for the report from results/*.csv.

Every chart shows the median of the repeated runs; where runs were repeated, the bars or
the shaded band show the minimum and maximum. Charts go to results/plots/*.png, and
results/summary.md gets the same numbers as tables. Missing CSV files are skipped.

    python plots.py
"""

import argparse
import csv
import statistics
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

RESULTS_DIR = Path(__file__).resolve().parent / "results"
PLOTS_DIR = RESULTS_DIR / "plots"

# Series colors in a fixed order (blue, orange, aqua, yellow); every series also has its own
# marker and line style, so the charts stay readable in black-and-white print.
COLORS = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100"]
MARKERS = ["o", "s", "^", "D"]
LINES = ["-", "--", "-.", ":"]
TEXT = "#52514e"
GRID = "#e4e3df"

plt.rcParams.update({
    "figure.figsize": (7, 4.2),
    "figure.dpi": 150,
    "font.size": 10,
    "axes.edgecolor": TEXT,
    "axes.labelcolor": TEXT,
    "axes.titlesize": 11,
    "axes.titleweight": "bold",
    "axes.spines.top": False,
    "axes.spines.right": False,
    "axes.grid": True,
    "axes.axisbelow": True,
    "grid.color": GRID,
    "grid.linewidth": 0.8,
    "xtick.color": TEXT,
    "ytick.color": TEXT,
    "legend.frameon": False,
    "lines.linewidth": 2,
    "lines.markersize": 6,
})

summary = []


def read_csv(name):
    path = RESULTS_DIR / name
    if not path.exists():
        print(f"skip: {path} not found")
        return None
    with open(path) as file:
        return list(csv.DictReader(file))


def group(rows, key_fields, value_field):
    """{(key values...): [values]} for rows with the same configuration."""
    groups = defaultdict(list)
    for row in rows:
        key = tuple(row[field] for field in key_fields)
        groups[key].append(float(row[value_field]))
    return groups


def med(values):
    return statistics.median(values)


def line(ax, index, xs, values, label):
    """One series: median line with markers and a min-max band."""
    medians = [med(v) for v in values]
    ax.plot(xs, medians, color=COLORS[index], marker=MARKERS[index], linestyle=LINES[index], label=label)
    if any(len(v) > 1 for v in values):
        ax.fill_between(xs, [min(v) for v in values], [max(v) for v in values], color=COLORS[index], alpha=0.15,
                        linewidth=0)
    return medians


def save(fig, name):
    PLOTS_DIR.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    fig.savefig(PLOTS_DIR / name)
    plt.close(fig)
    print(f"saved {PLOTS_DIR / name}")


def table(title, header, rows):
    summary.append(f"## {title}\n")
    summary.append("| " + " | ".join(header) + " |")
    summary.append("|" + "---|" * len(header))
    for row in rows:
        summary.append("| " + " | ".join(str(cell) for cell in row) + " |")
    summary.append("")


def plot_e1():
    rows = read_csv("e1_threads.csv")
    if rows is None:
        return
    times = group(rows, ["threads"], "time_ms")
    threads = sorted(int(key[0]) for key in times)
    values = [[t / 1000 for t in times[(str(n),)]] for n in threads]
    base = med(values[0]) * threads[0]

    fig, ax = plt.subplots()
    medians = line(ax, 0, threads, values, "медіана, мін-макс")
    ax.set(title="E1. Час побудови індексу від кількості потоків", xlabel="потоки", ylabel="час, с", xticks=threads)
    ax.set_ylim(bottom=0)
    save(fig, "e1_time.png")

    speedups = [base / m for m in medians]
    fig, ax = plt.subplots()
    ax.plot(threads, threads, color=TEXT, linestyle=":", linewidth=1.2, label="ідеальне (S = n)")
    ax.plot(threads, speedups, color=COLORS[0], marker=MARKERS[0], label="виміряне")
    for n, s in zip(threads, speedups):
        ax.annotate(f"{s:.2f}", (n, s), textcoords="offset points", xytext=(0, 8), ha="center", color=TEXT, fontsize=9)
    ax.set(title="E1. Прискорення S = T1 / Tn", xlabel="потоки", ylabel="прискорення", xticks=threads)
    ax.set_ylim(bottom=0)
    ax.legend()
    save(fig, "e1_speedup.png")

    efficiency = [s * threads[0] / n for s, n in zip(speedups, threads)]
    fig, ax = plt.subplots()
    ax.plot(threads, efficiency, color=COLORS[0], marker=MARKERS[0])
    ax.set(title="E1. Ефективність E = S / n", xlabel="потоки", ylabel="ефективність", xticks=threads)
    ax.set_ylim(0, 1.1)
    save(fig, "e1_efficiency.png")

    table("E1. Час побудови від кількості потоків", ["потоки", "медіана, мс", "прискорення", "ефективність"],
          [[n, f"{m * 1000:.0f}", f"{s:.2f}", f"{e:.2f}"] for n, m, s, e in zip(threads, medians, speedups, efficiency)])


def plot_e2_keyword():
    rows = read_csv("e2_rest_keyword.csv")
    if rows is None:
        return
    users = [int(row["users"]) for row in rows]

    fig, ax = plt.subplots()
    ax.plot(users, [float(row["rps"]) for row in rows], color=COLORS[0], marker=MARKERS[0])
    ax.set(title="E2a. Пропускна здатність REST keyword", xlabel="одночасні клієнти", ylabel="запитів/с")
    ax.set_xscale("log", base=2)
    ax.set_xticks(users, [str(u) for u in users])
    ax.set_ylim(bottom=0)
    save(fig, "e2a_throughput.png")

    fig, ax = plt.subplots()
    ax.plot(users, [float(row["p50_ms"]) for row in rows], color=COLORS[0], marker=MARKERS[0], label="p50")
    ax.plot(users, [float(row["p95_ms"]) for row in rows], color=COLORS[1], marker=MARKERS[1], linestyle=LINES[1],
            label="p95")
    ax.set(title="E2a. Затримка REST keyword", xlabel="одночасні клієнти", ylabel="затримка, мс")
    ax.set_xscale("log", base=2)
    ax.set_xticks(users, [str(u) for u in users])
    ax.set_ylim(bottom=0)
    ax.legend()
    save(fig, "e2a_latency.png")

    table("E2a. REST keyword", ["клієнти", "запитів/с", "p50, мс", "p95, мс", "p99, мс", "помилки", "CPU клієнта, ядер"],
          [[row["users"], f"{float(row['rps']):.1f}", f"{float(row['p50_ms']):.0f}", f"{float(row['p95_ms']):.0f}",
            f"{float(row['p99_ms']):.0f}", row["errors"], row["client_cpu_cores"]] for row in rows])


def plot_e2_llm():
    rest = read_csv("e2_rest_expanded.csv")
    ws = read_csv("e2_ws.csv")
    if rest is None and ws is None:
        return
    series = []
    if rest is not None:
        series.append(("REST expanded", [int(r["users"]) for r in rest], [float(r["rps"]) for r in rest],
                       [float(r["p50_ms"]) / 1000 for r in rest]))
    if ws is not None:
        series.append(("WebSocket", [int(r["users"]) for r in ws], [float(r["qps"]) for r in ws],
                       [float(r["p50_ms"]) / 1000 for r in ws]))

    fig, ax = plt.subplots()
    for index, (label, users, throughput, _) in enumerate(series):
        ax.plot(users, throughput, color=COLORS[index], marker=MARKERS[index], linestyle=LINES[index], label=label)
    ax.set(title="E2b. Пропускна здатність з LLM", xlabel="одночасні клієнти", ylabel="запитів/с")
    ax.set_ylim(bottom=0)
    ax.legend()
    save(fig, "e2b_throughput.png")

    fig, ax = plt.subplots()
    for index, (label, users, _, latency) in enumerate(series):
        ax.plot(users, latency, color=COLORS[index], marker=MARKERS[index], linestyle=LINES[index], label=label)
    ax.set(title="E2b. Медіанна затримка з LLM", xlabel="одночасні клієнти", ylabel="затримка p50, с")
    ax.set_ylim(bottom=0)
    ax.legend()
    save(fig, "e2b_latency.png")

    if ws is not None:
        table("E2b. WebSocket", ["клієнти", "запитів/с", "p50, мс", "p95, мс", "LLM p50, мс", "пошук, мс", "без розширення"],
              [[r["users"], f"{float(r['qps']):.2f}", f"{float(r['p50_ms']):.0f}", f"{float(r['p95_ms']):.0f}",
                f"{float(r['expansion_p50_ms']):.0f}", f"{float(r['search_avg_ms']):.0f}", r["no_expansion"]]
               for r in ws])
    if rest is not None:
        table("E2b. REST expanded", ["клієнти", "запитів/с", "p50, мс", "p95, мс", "без розширення", "помилки"],
              [[r["users"], f"{float(r['rps']):.2f}", f"{float(r['p50_ms']):.0f}", f"{float(r['p95_ms']):.0f}",
                r["no_expansion"], r["errors"]] for r in rest])


def plot_e3():
    rows = read_csv("e3.csv")
    if rows is None:
        return
    times = group(rows, ["threads", "shards"], "time_ms")
    thread_values = sorted({int(key[0]) for key in times})
    shard_values = sorted({int(key[1]) for key in times})

    fig, ax = plt.subplots()
    table_rows = []
    for index, threads in enumerate(thread_values):
        values = [[t / 1000 for t in times[(str(threads), str(s))]] for s in shard_values]
        medians = line(ax, index, shard_values, values, f"{threads} потік(и)")
        table_rows += [[threads, s, f"{m * 1000:.0f}"] for s, m in zip(shard_values, medians)]
    ax.set(title="E3. Час побудови від кількості шардів (25 % корпусу)", xlabel="шарди", ylabel="час, с")
    ax.set_xscale("log", base=2)
    ax.set_xticks(shard_values, [str(s) for s in shard_values])
    ax.set_ylim(bottom=0)
    ax.legend()
    save(fig, "e3_shards.png")
    table("E3. Шарди", ["потоки", "шарди", "медіана, мс"], table_rows)


def plot_e4():
    rows = read_csv("e4.csv")
    if rows is None:
        return
    times = group(rows, ["threads", "documents"], "time_ms")
    thread_values = sorted({int(key[0]) for key in times})
    documents = sorted({int(key[1]) for key in times})

    fig, ax = plt.subplots()
    medians_by_threads = {}
    for index, threads in enumerate(thread_values):
        values = [[t / 1000 for t in times[(str(threads), str(d))]] for d in documents]
        medians_by_threads[threads] = line(ax, index, documents, values, f"{threads} потік(и)")
    ax.set(title="E4. Час побудови від розміру корпусу", xlabel="документи", ylabel="час, с")
    ax.set_ylim(bottom=0)
    ax.legend()
    save(fig, "e4_size.png")

    table_rows = []
    if len(thread_values) >= 2:
        low, high = thread_values[0], thread_values[-1]
        speedups = [a / b for a, b in zip(medians_by_threads[low], medians_by_threads[high])]
        fig, ax = plt.subplots()
        ax.plot(documents, speedups, color=COLORS[0], marker=MARKERS[0])
        ax.set(title=f"E4. Прискорення {high} потоків відносно {low} від розміру корпусу", xlabel="документи",
               ylabel="прискорення")
        ax.set_ylim(bottom=0)
        save(fig, "e4_speedup.png")
        table_rows = [[d, f"{a * 1000:.0f}", f"{b * 1000:.0f}", f"{s:.2f}"]
                      for d, a, b, s in zip(documents, medians_by_threads[low], medians_by_threads[high], speedups)]
        table(f"E4. Розмір корпусу", ["документи", f"{low} потік, мс", f"{high} потоків, мс", "прискорення"], table_rows)


def plot_e6():
    rows = read_csv("e6.csv")
    if rows is None:
        return
    times = group(rows, ["shards", "per_token_lock", "threads"], "time_ms")
    locks = group(rows, ["shards", "per_token_lock", "threads"], "locks")
    thread_values = sorted({int(key[2]) for key in times})
    variants = [("64", "0", "локальна таблиця, 64 шарди"), ("1", "0", "локальна таблиця, 1 шард"),
                ("64", "1", "кожен токен, 64 шарди"), ("1", "1", "кожен токен, 1 шард")]

    fig, ax = plt.subplots()
    table_rows = []
    for index, (shards, per_token, label) in enumerate(variants):
        if (shards, per_token, str(thread_values[0])) not in times:
            continue
        values = [[t / 1000 for t in times[(shards, per_token, str(n))]] for n in thread_values]
        medians = line(ax, index, thread_values, values, label)
        lock_count = int(locks[(shards, per_token, str(thread_values[0]))][0])
        table_rows += [[label, n, f"{m * 1000:.0f}", f"{lock_count:,}".replace(",", " ")]
                       for n, m in zip(thread_values, medians)]
    ax.set(title="E6. Локальна таблиця проти запису кожного токена (10 % корпусу)", xlabel="потоки", ylabel="час, с",
           xticks=thread_values)
    ax.set_ylim(bottom=0)
    ax.legend()
    save(fig, "e6_locking.png")
    table("E6. Гранулярність блокувань", ["варіант", "потоки", "медіана, мс", "захоплень блокувань"], table_rows)


def plot_e7():
    rows = read_csv("e7.csv")
    if rows is None:
        return
    times = group(rows, ["largest_first", "threads"], "time_ms")
    idle = group(rows, ["largest_first", "threads"], "idle")
    thread_values = sorted({int(key[1]) for key in times})
    variants = [("1", "спершу найбільші (LPT)"), ("0", "за шляхом файлу")]

    fig, ax = plt.subplots()
    for index, (flag, label) in enumerate(variants):
        values = [[t / 1000 for t in times[(flag, str(n))]] for n in thread_values]
        line(ax, index, thread_values, values, label)
    ax.set(title="E7. Час побудови від порядку файлів (25 % корпусу)", xlabel="потоки", ylabel="час, с",
           xticks=thread_values)
    ax.set_ylim(bottom=0)
    ax.legend()
    save(fig, "e7_time.png")

    fig, ax = plt.subplots()
    table_rows = []
    for index, (flag, label) in enumerate(variants):
        values = [[v * 100 for v in idle[(flag, str(n))]] for n in thread_values]
        medians = line(ax, index, thread_values, values, label)
        time_medians = [med(times[(flag, str(n))]) for n in thread_values]
        table_rows += [[label, n, f"{t:.0f}", f"{m:.1f}"] for n, t, m in zip(thread_values, time_medians, medians)]
    ax.set(title="E7. Частка простою потоків", xlabel="потоки", ylabel="простій, %", xticks=thread_values)
    ax.set_ylim(bottom=0)
    ax.legend()
    save(fig, "e7_idle.png")
    table("E7. Порядок файлів", ["порядок", "потоки", "медіана, мс", "простій, %"], table_rows)


def plot_e8():
    rows = read_csv("e8.csv")
    if rows is None:
        return
    with_stem = [row for row in rows if row["stemming"] == "1"]
    without_stem = [row for row in rows if row["stemming"] == "0"]
    if not with_stem or not without_stem:
        return
    queries = [field[len("hits:"):] for field in rows[0] if field.startswith("hits:")]

    fig, ax = plt.subplots(figsize=(8, 4.2))
    width = 0.38
    positions = range(len(queries))
    for index, (label, part) in enumerate([("зі стемінгом", with_stem), ("без стемінгу", without_stem)]):
        hits = [int(part[0][f"hits:{query}"]) for query in queries]
        shift = -width / 2 - 0.01 if index == 0 else width / 2 + 0.01
        ax.bar([p + shift for p in positions], hits, width=width, color=COLORS[index], label=label,
               hatch=None if index == 0 else "//", edgecolor="white", linewidth=0.5)
    ax.set_xticks(list(positions), queries, rotation=30, ha="right")
    ax.set(title="E8. Кількість знайдених документів (25 % корпусу)", ylabel="документи")
    ax.grid(axis="x", visible=False)
    ax.legend()
    save(fig, "e8_hits.png")

    labels = ["зі стемінгом", "без стемінгу"]
    times = [[float(row["time_ms"]) / 1000 for row in part] for part in (with_stem, without_stem)]
    medians = [med(values) for values in times]
    errors = [[m - min(v) for m, v in zip(medians, times)], [max(v) - m for m, v in zip(medians, times)]]
    fig, ax = plt.subplots(figsize=(5, 4.2))
    ax.bar(labels, medians, width=0.5, color=COLORS[:2], hatch=[None, "//"], edgecolor="white", linewidth=0.5,
           yerr=errors, ecolor=TEXT, capsize=4)
    for x, m in enumerate(medians):
        ax.annotate(f"{m:.1f} с", (x, m), textcoords="offset points", xytext=(0, 10), ha="center", color=TEXT)
    ax.set(title="E8. Час побудови (25 % корпусу, 8 потоків)", ylabel="час, с")
    ax.grid(axis="x", visible=False)
    ax.set_ylim(0, max(medians) * 1.2)
    save(fig, "e8_time.png")

    terms = [int(part[0]["terms"]) for part in (with_stem, without_stem)]
    fig, ax = plt.subplots(figsize=(5, 4.2))
    ax.bar(labels, [t / 1000 for t in terms], width=0.5, color=COLORS[:2], hatch=[None, "//"], edgecolor="white",
           linewidth=0.5)
    for x, t in enumerate(terms):
        ax.annotate(f"{t / 1000:.0f} тис.", (x, t / 1000), textcoords="offset points", xytext=(0, 4), ha="center",
                    color=TEXT)
    ax.set(title="E8. Розмір словника (25 % корпусу)", ylabel="унікальних термінів, тис.")
    ax.grid(axis="x", visible=False)
    ax.set_ylim(0, max(terms) / 1000 * 1.15)
    save(fig, "e8_terms.png")

    def describe(part):
        return [f"{med([float(row['time_ms']) for row in part]):.0f}", part[0]["terms"]]

    table("E8. Стемінг", ["варіант", "медіана, мс", "унікальних термінів"],
          [["зі стемінгом"] + describe(with_stem), ["без стемінгу"] + describe(without_stem)])


def main():
    global RESULTS_DIR, PLOTS_DIR
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--results", default=RESULTS_DIR, type=Path, help="folder with the CSV files")
    args = parser.parse_args()
    RESULTS_DIR = args.results
    PLOTS_DIR = RESULTS_DIR / "plots"

    plot_e1()
    plot_e2_keyword()
    plot_e2_llm()
    plot_e3()
    plot_e4()
    plot_e6()
    plot_e7()
    plot_e8()
    if summary:
        (RESULTS_DIR / "summary.md").write_text("# Результати бенчмарків\n\n" + "\n".join(summary) + "\n")
        print(f"saved {RESULTS_DIR / 'summary.md'}")


if __name__ == "__main__":
    main()
