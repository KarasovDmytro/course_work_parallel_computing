#pragma once
#include <mutex>
#include <string>
#include "index/hashTable.h"
#include "index/postingList.h"

class Shard {
private:
    HashTable<PostingList> table;
    std::mutex tableMutex;

public:
    void addPosting(const std::string& term, unsigned int hash, int docId, int tf);
    const PostingList* find(const std::string& term, unsigned int hash) const;
    int termCount() const;
    void clear();
};
