#pragma once
#include <string>
#include <vector>
#include "index/invertedIndex.h"
#include "text/tokenizer.h"

struct QueryTerm {
    std::string term;
    unsigned int hash;
    double weight;
};

struct SearchResult {
    std::string docId;
    double score;
    std::string snippet;
};

struct SearchOutcome {
    bool ready;
    std::vector<SearchResult> results;
};

class SearchService {
private:
    InvertedIndex& index;
    Tokenizer tokenizer;

    void addTerms(std::vector<QueryTerm>& terms, const std::string& text, double weight) const;
    std::string makeSnippet(const std::string& path, const std::string& term) const;

public:
    SearchService(InvertedIndex& index);

    bool isReady() const;
    SearchOutcome search(const std::string& query, const std::vector<std::string>& expandedWords, int limit) const;
};
