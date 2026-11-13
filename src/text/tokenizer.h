#pragma once
#include <string>
#include <vector>
#include "text/stemmer.h"

class Tokenizer {
private:
    Stemmer stemmer;

    bool isTokenChar(unsigned char c) const;
    char toLower(unsigned char c) const;

public:
    std::vector<std::string> tokenize(const std::string& text) const;
};
