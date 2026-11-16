#include "shard.h"

using namespace std;

void Shard::addPosting(const string& term, unsigned int hash, int docId, int tf) {
    lock_guard<mutex> lock(tableMutex);
    locksTaken++;
    table.getOrCreate(term, hash).add(docId, tf);
}

const PostingList* Shard::find(const string& term, unsigned int hash) const {
    return table.find(term, hash);
}

int Shard::termCount() const {
    return table.size();
}

long long Shard::lockCount() const {
    return locksTaken;
}

void Shard::clear() {
    lock_guard<mutex> lock(tableMutex);
    table = HashTable<PostingList>();
    locksTaken = 0;
}
