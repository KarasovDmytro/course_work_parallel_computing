#include "tokenizer.h"

using namespace std;

bool Tokenizer::isTokenChar(unsigned char c) const {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

char Tokenizer::toLower(unsigned char c) const {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 'a';
    }
    return c;
}

vector<string> Tokenizer::tokenize(const string& text) const {
    vector<string> terms;
    string current;

    for (int i = 0; i < (int)text.size(); i++) {
        unsigned char c = text[i];

        if (isTokenChar(c)) {
            current += toLower(c);
        } else if (!current.empty()) {
            terms.push_back(stemmer.stem(current));
            current.clear();
        }
    }

    if (!current.empty()) {
        terms.push_back(stemmer.stem(current));
    }
    return terms;
}
