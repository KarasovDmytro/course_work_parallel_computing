#pragma once
#include <mutex>
#include <string>
#include <vector>
#include "index/invertedIndex.h"
#include "text/tokenizer.h"

struct BuildResult {
    bool started;
    int documents;
    int timeMs;
};

struct BuildStats {
    int documents = 0;
    int buildTimeMs = 0;
    int threadsUsed = 0;
};

class IndexBuilder {
private:
    InvertedIndex& index;
    Tokenizer tokenizer;
    std::string dataDir;

    BuildStats lastStats;
    mutable std::mutex statsMutex;

    std::vector<std::string> findFiles() const;
    void indexFile(int docId, const std::string& path);

public:
    IndexBuilder(InvertedIndex& index, const std::string& dataDir);

    BuildResult build(int threadCount);
    BuildStats stats() const;
};
