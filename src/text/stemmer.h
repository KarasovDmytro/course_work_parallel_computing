#pragma once
#include <string>
#include <vector>

struct SuffixRule {
    std::string suffix;
    std::string replacement;
};

class Stemmer {
private:
    bool isConsonant(const std::string& word, int i) const;
    int measure(const std::string& word, int length) const;
    bool hasVowel(const std::string& word, int length) const;
    bool endsWithDoubleConsonant(const std::string& word, int length) const;
    bool endsWithCvc(const std::string& word, int length) const;
    bool endsWith(const std::string& word, const std::string& suffix) const;
    int findLongestRule(const std::string& word, const std::vector<SuffixRule>& rules) const;

    void step1a(std::string& word) const;
    void step1b(std::string& word) const;
    void step1c(std::string& word) const;
    void step2(std::string& word) const;
    void step3(std::string& word) const;
    void step4(std::string& word) const;
    void step5(std::string& word) const;

public:
    std::string stem(const std::string& word) const;
};
