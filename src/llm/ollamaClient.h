#pragma once
#include <string>

using namespace std;

class OllamaClient {
private:
    string baseUrl;
    string model;

    bool hasModel(const string& tagsJson) const;

public:
    OllamaClient(const string& baseUrl, const string& model);

    bool isAvailable() const;
};
