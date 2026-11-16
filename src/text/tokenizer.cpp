#include "tokenizer.h"

using namespace std;

Tokenizer::Tokenizer(bool useStemming) : useStemming(useStemming) {}

bool Tokenizer::isTokenChar(unsigned char c) const {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

char Tokenizer::toLower(unsigned char c) const {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 'a';
    }
    return c;
}

bool Tokenizer::nextToken(const string& text, int& position, Token& token) const {
    int size = (int)text.size();

    while (position < size && !isTokenChar(text[position])) {
        position++;
    }
    if (position >= size) {
        return false;
    }

    token.start = position;
    string word;
    while (position < size && isTokenChar(text[position])) {
        word += toLower(text[position]);
        position++;
    }
    token.end = position;
    if (useStemming) {
        token.term = stemmer.stem(word);
    } else {
        token.term = word;
    }
    return true;
}

vector<string> Tokenizer::tokenize(const string& text) const {
    vector<string> terms;
    int position = 0;
    Token token;

    while (nextToken(text, position, token)) {
        terms.push_back(token.term);
    }
    return terms;
}
