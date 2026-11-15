#include "indexBuilder.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include "common/fileReader.h"
#include "concurrency/threadPool.h"
#include "index/hashTable.h"
#include "index/termHash.h"

using namespace std;

IndexBuilder::IndexBuilder(InvertedIndex& index, const string& dataDir) : index(index), dataDir(dataDir) {
}

vector<string> IndexBuilder::findFiles() const {
    vector<string> paths;
    error_code error;
    filesystem::recursive_directory_iterator it(dataDir, error);
    filesystem::recursive_directory_iterator end;

    while (!error && it != end) {
        if (it->is_regular_file(error) && it->path().extension() == ".txt") {
            paths.push_back(it->path().string());
        }
        it.increment(error);
    }

    sort(paths.begin(), paths.end());
    return paths;
}

void IndexBuilder::indexFile(int docId, const string& path) {
    string text = readFile(path);
    vector<string> terms = tokenizer.tokenize(text);
    HashTable<int> counts;
    for (int i = 0; i < (int)terms.size(); i++) {
        counts.getOrCreate(terms[i], termHash(terms[i]))++;
    }

    for (int i = 0; i < counts.capacity(); i++) {
        const HashEntry<int>& entry = counts.slot(i);
        if (entry.used) {
            index.addPosting(entry.key, entry.hash, docId, entry.value);
        }
    }
}

BuildResult IndexBuilder::build(int threadCount) {
    chrono::steady_clock::time_point start = chrono::steady_clock::now();

    vector<string> paths = findFiles();
    if (!index.beginBuild(paths)) {
        return {false, 0, 0};
    }

    vector<long long> sizes(paths.size());
    vector<int> order(paths.size());
    for (int i = 0; i < (int)paths.size(); i++) {
        error_code error;
        sizes[i] = filesystem::file_size(paths[i], error);
        order[i] = i;
    }
    sort(order.begin(), order.end(), [&sizes](int a, int b) {
        return sizes[a] > sizes[b];
    });

    int failed = 0;
    int threadsUsed = 0;
    try {
        ThreadPool pool(threadCount);
        for (int i = 0; i < (int)order.size(); i++) {
            int docId = order[i];
            pool.submit([this, docId, &paths] {
                indexFile(docId, paths[docId]);
            });
        }
        pool.waitIdle();
        failed = pool.failedCount();
        threadsUsed = pool.threadCount();
    } catch (...) {
        index.finishBuild(false);
        throw;
    }
    index.finishBuild(true);

    int documents = (int)paths.size() - failed;
    int timeMs = (int)chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start).count();

    {
        lock_guard<mutex> lock(statsMutex);
        lastStats.documents = documents;
        lastStats.buildTimeMs = timeMs;
        lastStats.threadsUsed = threadsUsed;
    }
    return {true, documents, timeMs};
}

BuildStats IndexBuilder::stats() const {
    lock_guard<mutex> lock(statsMutex);
    return lastStats;
}
