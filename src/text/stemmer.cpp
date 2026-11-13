#include "stemmer.h"

using namespace std;

const vector<SuffixRule> step2Rules = {
    {"ational", "ate"}, {"tional", "tion"}, {"enci", "ence"},  {"anci", "ance"},
    {"izer", "ize"},    {"bli", "ble"},     {"alli", "al"},    {"entli", "ent"},
    {"eli", "e"},       {"ousli", "ous"},   {"ization", "ize"}, {"ation", "ate"},
    {"ator", "ate"},    {"alism", "al"},    {"iveness", "ive"}, {"fulness", "ful"},
    {"ousness", "ous"}, {"aliti", "al"},    {"iviti", "ive"},  {"biliti", "ble"},
    {"logi", "log"},
};

const vector<SuffixRule> step3Rules = {
    {"icate", "ic"}, {"ative", ""}, {"alize", "al"}, {"iciti", "ic"},
    {"ical", "ic"},  {"ful", ""},   {"ness", ""},
};

const vector<SuffixRule> step4Rules = {
    {"al", ""},  {"ance", ""}, {"ence", ""}, {"er", ""},  {"ic", ""},   {"able", ""},
    {"ible", ""}, {"ant", ""}, {"ement", ""}, {"ment", ""}, {"ent", ""}, {"ion", ""},
    {"ou", ""},  {"ism", ""},  {"ate", ""},  {"iti", ""}, {"ous", ""},  {"ive", ""},
    {"ize", ""},
};

bool Stemmer::isConsonant(const string& word, int i) const {
    char c = word[i];

    if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') {
        return false;
    }
    if (c == 'y') {
        if (i == 0) {
            return true;
        }
        return !isConsonant(word, i - 1);
    }
    return true;
}

int Stemmer::measure(const string& word, int length) const {
    int m = 0;
    int i = 0;

    while (i < length && isConsonant(word, i)) {
        i++;
    }

    while (i < length) {
        while (i < length && !isConsonant(word, i)) {
            i++;
        }
        if (i >= length) {
            break;
        }

        while (i < length && isConsonant(word, i)) {
            i++;
        }
        m++;
    }
    return m;
}

bool Stemmer::hasVowel(const string& word, int length) const {
    for (int i = 0; i < length; i++) {
        if (!isConsonant(word, i)) {
            return true;
        }
    }
    return false;
}

bool Stemmer::endsWithDoubleConsonant(const string& word, int length) const {
    if (length < 2) {
        return false;
    }
    return word[length - 1] == word[length - 2] && isConsonant(word, length - 1);
}

bool Stemmer::endsWithCvc(const string& word, int length) const {
    if (length < 3) {
        return false;
    }
    if (!isConsonant(word, length - 3) || isConsonant(word, length - 2) || !isConsonant(word, length - 1)) {
        return false;
    }

    char last = word[length - 1];
    return last != 'w' && last != 'x' && last != 'y';
}

bool Stemmer::endsWith(const string& word, const string& suffix) const {
    if (suffix.size() > word.size()) {
        return false;
    }
    return word.compare(word.size() - suffix.size(), suffix.size(), suffix) == 0;
}

int Stemmer::findLongestRule(const string& word, const vector<SuffixRule>& rules) const {
    int best = -1;

    for (int i = 0; i < (int)rules.size(); i++) {
        if (!endsWith(word, rules[i].suffix)) {
            continue;
        }
        if (best == -1 || rules[i].suffix.size() > rules[best].suffix.size()) {
            best = i;
        }
    }
    return best;
}

void Stemmer::step1a(string& word) const {
    if (endsWith(word, "sses")) {
        word.erase(word.size() - 2);
    } else if (endsWith(word, "ies")) {
        word.erase(word.size() - 2);
    } else if (endsWith(word, "ss")) {
        return;
    } else if (endsWith(word, "s")) {
        word.pop_back();
    }
}

void Stemmer::step1b(string& word) const {
    bool removed = false;

    if (endsWith(word, "eed")) {
        if (measure(word, word.size() - 3) > 0) {
            word.pop_back();
        }
    } else if (endsWith(word, "ed") && hasVowel(word, word.size() - 2)) {
        word.erase(word.size() - 2);
        removed = true;
    } else if (endsWith(word, "ing") && hasVowel(word, word.size() - 3)) {
        word.erase(word.size() - 3);
        removed = true;
    }

    if (!removed) {
        return;
    }

    int length = word.size();
    if (endsWith(word, "at") || endsWith(word, "bl") || endsWith(word, "iz")) {
        word += 'e';
    } else if (endsWithDoubleConsonant(word, length)) {
        char last = word.back();
        if (last != 'l' && last != 's' && last != 'z') {
            word.pop_back();
        }
    } else if (measure(word, length) == 1 && endsWithCvc(word, length)) {
        word += 'e';
    }
}

void Stemmer::step1c(string& word) const {
    if (endsWith(word, "y") && hasVowel(word, word.size() - 1)) {
        word.back() = 'i';
    }
}

void Stemmer::step2(string& word) const {
    int rule = findLongestRule(word, step2Rules);
    if (rule == -1) {
        return;
    }

    int stemLength = word.size() - step2Rules[rule].suffix.size();
    if (measure(word, stemLength) > 0) {
        word = word.substr(0, stemLength) + step2Rules[rule].replacement;
    }
}

void Stemmer::step3(string& word) const {
    int rule = findLongestRule(word, step3Rules);
    if (rule == -1) {
        return;
    }

    int stemLength = word.size() - step3Rules[rule].suffix.size();
    if (measure(word, stemLength) > 0) {
        word = word.substr(0, stemLength) + step3Rules[rule].replacement;
    }
}

void Stemmer::step4(string& word) const {
    int rule = findLongestRule(word, step4Rules);
    if (rule == -1) {
        return;
    }

    int stemLength = word.size() - step4Rules[rule].suffix.size();
    if (step4Rules[rule].suffix == "ion") {
        if (stemLength == 0) {
            return;
        }
        char last = word[stemLength - 1];
        if (last != 's' && last != 't') {
            return;
        }
    }

    if (measure(word, stemLength) > 1) {
        word.erase(stemLength);
    }
}

void Stemmer::step5(string& word) const {
    int length = word.size();

    if (word.back() == 'e') {
        int m = measure(word, length - 1);
        if (m > 1 || (m == 1 && !endsWithCvc(word, length - 1))) {
            word.pop_back();
        }
    }

    length = word.size();
    if (word.back() == 'l' && endsWithDoubleConsonant(word, length) && measure(word, length) > 1) {
        word.pop_back();
    }
}

string Stemmer::stem(const string& word) const {
    if (word.size() <= 2) {
        return word;
    }

    string result = word;
    step1a(result);
    step1b(result);
    step1c(result);
    step2(result);
    step3(result);
    step4(result);
    step5(result);
    return result;
}
