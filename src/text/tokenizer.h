#pragma once
#include <string>
#include <vector>
#include "text/stemmer.h"

struct Token {
    std::string term;
    int start;
    int end;
};

class Tokenizer {
private:
    Stemmer stemmer;
    bool useStemming;

    bool isTokenChar(unsigned char c) const;
    char toLower(unsigned char c) const;

public:
    Tokenizer(bool useStemming = true);

    bool nextToken(const std::string& text, int& position, Token& token) const;
    std::vector<std::string> tokenize(const std::string& text) const;
};
