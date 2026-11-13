#pragma once
#include <vector>

struct Posting {
    int docId;
    int tf;
};

class PostingList {
private:
    std::vector<Posting> postings;

public:
    void add(int docId, int tf);
    int size() const;
    const Posting& get(int i) const;
};
