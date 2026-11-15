#include "searchService.h"
#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <shared_mutex>
#include "common/fileReader.h"
#include "index/termHash.h"

using namespace std;

const double originalWeight = 1.0;
const double expandedWeight = 0.5;
const int snippetContext = 100;

static bool isSpace(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

static bool isUtf8Continuation(char c) {
    return ((unsigned char)c & 0xC0) == 0x80;
}

SearchService::SearchService(InvertedIndex& index) : index(index) {
}

bool SearchService::isReady() const {
    return index.getState() == IndexState::Ready;
}

void SearchService::addTerms(vector<QueryTerm>& terms, const string& text, double weight) const {
    vector<string> words = tokenizer.tokenize(text);

    for (int i = 0; i < (int)words.size(); i++) {
        bool found = false;
        for (int j = 0; j < (int)terms.size(); j++) {
            if (terms[j].term == words[i]) {
                if (weight > terms[j].weight) {
                    terms[j].weight = weight;
                }
                found = true;
                break;
            }
        }

        if (!found) {
            terms.push_back({words[i], termHash(words[i]), weight});
        }
    }
}

string SearchService::makeSnippet(const string& path, const string& term) const {
    string text;
    try {
        text = readFile(path);
    } catch (const exception&) {
        return "";
    }

    int size = (int)text.size();
    int matchStart = 0;
    int matchEnd = 0;
    bool found = false;
    int position = 0;
    Token token;

    while (!found && tokenizer.nextToken(text, position, token)) {
        if (token.term == term) {
            matchStart = token.start;
            matchEnd = token.end;
            found = true;
        }
    }

    int start = max(0, matchStart - snippetContext);
    int end = min(size, matchEnd + snippetContext);
    if (!found) {
        end = min(size, 2 * snippetContext);
    }

    if (start > 0) {
        int i = start;
        while (i < matchStart && !isSpace(text[i])) {
            i++;
        }
        if (i < matchStart) {
            start = i + 1;
        }
    }
    if (end < size) {
        int i = end;
        while (i > matchEnd && !isSpace(text[i])) {
            i--;
        }
        if (i > matchEnd) {
            end = i;
        }
    }

    while (start > 0 && isUtf8Continuation(text[start])) {
        start--;
    }
    while (end < size && isUtf8Continuation(text[end])) {
        end++;
    }

    string snippet;
    if (start > 0) {
        snippet += "...";
    }
    bool lastWasSpace = false;
    for (int i = start; i < end; i++) {
        if (isSpace(text[i])) {
            if (!lastWasSpace && !snippet.empty()) {
                snippet += ' ';
            }
            lastWasSpace = true;
        } else {
            snippet += text[i];
            lastWasSpace = false;
        }
    }
    if (end < size) {
        snippet += "...";
    }
    return snippet;
}

SearchOutcome SearchService::search(const string& query, const vector<string>& expandedWords, int limit) const {
    SearchOutcome outcome;
    outcome.ready = false;

    shared_lock<shared_mutex> lock = index.tryLockForReading();
    if (!lock.owns_lock()) {
        return outcome;
    }
    outcome.ready = true;

    vector<QueryTerm> terms;
    addTerms(terms, query, originalWeight);
    for (int i = 0; i < (int)expandedWords.size(); i++) {
        addTerms(terms, expandedWords[i], expandedWeight);
    }

    int documentCount = index.documentCount();
    vector<double> scores(documentCount, 0.0);
    vector<bool> matched(documentCount, false);
    vector<int> bestTerm(documentCount, -1);
    vector<double> bestContribution(documentCount, -1.0);
    vector<int> candidates;

    for (int i = 0; i < (int)terms.size(); i++) {
        const PostingList* postings = index.find(terms[i].term, terms[i].hash);
        if (postings == nullptr) {
            continue;
        }

        double idf = log10((double)documentCount / postings->size());
        for (int j = 0; j < postings->size(); j++) {
            const Posting& posting = postings->get(j);
            double contribution = terms[i].weight * (1 + log10(posting.tf)) * idf;
            scores[posting.docId] += contribution;

            if (contribution > bestContribution[posting.docId]) {
                bestContribution[posting.docId] = contribution;
                bestTerm[posting.docId] = i;
            }

            if (!matched[posting.docId]) {
                matched[posting.docId] = true;
                candidates.push_back(posting.docId);
            }
        }
    }

    int count = min(limit, (int)candidates.size());
    partial_sort(candidates.begin(), candidates.begin() + count, candidates.end(), [&scores](int a, int b) {
        if (scores[a] != scores[b]) {
            return scores[a] > scores[b];
        }
        return a < b;
    });

    for (int i = 0; i < count; i++) {
        int docId = candidates[i];
        const string& path = index.documentPath(docId);

        SearchResult result;
        result.docId = filesystem::path(path).stem().string();
        result.score = scores[docId];
        result.snippet = makeSnippet(path, terms[bestTerm[docId]].term);
        outcome.results.push_back(result);
    }
    return outcome;
}
