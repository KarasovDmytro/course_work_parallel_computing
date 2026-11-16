#include "invertedIndex.h"

using namespace std;

static int roundUpToPowerOfTwo(int n) {
    int result = 1;
    while (result < n) {
        result *= 2;
    }
    return result;
}

InvertedIndex::InvertedIndex(int shardCount) : shards(roundUpToPowerOfTwo(shardCount)) {
    int bits = 0;
    while ((1 << bits) < (int)shards.size()) {
        bits++;
    }
    shardShift = 32 - bits;
}

int InvertedIndex::shardFor(unsigned int hash) const {
    if (shards.size() == 1) {
        return 0;
    }
    return hash >> shardShift;
}

bool InvertedIndex::beginBuild(const vector<string>& paths) {
    IndexState expected = IndexState::Empty;
    if (!state.compare_exchange_strong(expected, IndexState::Building)) {
        expected = IndexState::Ready;
        if (!state.compare_exchange_strong(expected, IndexState::Building)) {
            return false;
        }
    }

    readMutex.lock();
    for (int i = 0; i < (int)shards.size(); i++) {
        shards[i].clear();
    }
    docPaths = paths;
    return true;
}

void InvertedIndex::addPosting(const string& term, unsigned int hash, int docId, int tf) {
    shards[shardFor(hash)].addPosting(term, hash, docId, tf);
}

void InvertedIndex::finishBuild(bool success) {
    if (success) {
        state = IndexState::Ready;
    } else {
        for (int i = 0; i < (int)shards.size(); i++) {
            shards[i].clear();
        }
        docPaths.clear();
        state = IndexState::Empty;
    }
    readMutex.unlock();
}

shared_lock<shared_mutex> InvertedIndex::tryLockForReading() {
    if (state != IndexState::Ready) {
        return shared_lock<shared_mutex>();
    }

    shared_lock<shared_mutex> lock(readMutex, try_to_lock);
    if (lock.owns_lock() && state != IndexState::Ready) {
        lock.unlock();
    }
    return lock;
}

IndexState InvertedIndex::getState() const {
    return state;
}

const PostingList* InvertedIndex::find(const string& term, unsigned int hash) const {
    return shards[shardFor(hash)].find(term, hash);
}

int InvertedIndex::documentCount() const {
    return (int)docPaths.size();
}

const string& InvertedIndex::documentPath(int docId) const {
    return docPaths[docId];
}

int InvertedIndex::termCount() const {
    int total = 0;
    for (int i = 0; i < (int)shards.size(); i++) {
        total += shards[i].termCount();
    }
    return total;
}

long long InvertedIndex::lockCount() const {
    long long total = 0;
    for (int i = 0; i < (int)shards.size(); i++) {
        total += shards[i].lockCount();
    }
    return total;
}

int InvertedIndex::shardCount() const {
    return (int)shards.size();
}
