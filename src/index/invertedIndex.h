#pragma once
#include <atomic>
#include <shared_mutex>
#include <string>
#include <vector>
#include "index/postingList.h"
#include "index/shard.h"

enum class IndexState { Empty, Building, Ready };

class InvertedIndex {
private:
    std::vector<Shard> shards;
    int shardShift;
    std::vector<std::string> docPaths;
    std::atomic<IndexState> state{IndexState::Empty};
    std::shared_mutex readMutex;

    int shardFor(unsigned int hash) const;

public:
    InvertedIndex(int shardCount = 64);

    InvertedIndex(const InvertedIndex&) = delete;
    InvertedIndex& operator=(const InvertedIndex&) = delete;

    bool beginBuild(const std::vector<std::string>& paths);
    void addPosting(const std::string& term, unsigned int hash, int docId, int tf);
    void finishBuild(bool success);

    std::shared_lock<std::shared_mutex> tryLockForReading();
    IndexState getState() const;
    const PostingList* find(const std::string& term, unsigned int hash) const;
    int documentCount() const;
    const std::string& documentPath(int docId) const;
    int termCount() const;
    long long lockCount() const;
    int shardCount() const;
};
