#pragma once
#include <string>

using namespace std;

class OllamaClient {
private:
    string baseUrl;

public:
    OllamaClient(const string& baseUrl);

    bool isAvailable() const;
};
