#include "postingList.h"

using namespace std;

void PostingList::add(int docId, int tf) {
    postings.push_back({docId, tf});
}

int PostingList::size() const {
    return (int)postings.size();
}

const Posting& PostingList::get(int i) const {
    return postings[i];
}
