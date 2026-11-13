#pragma once
#include <string>

class OllamaClient {
private:
    std::string baseUrl;
    std::string model;

    bool hasModel(const std::string& tagsJson) const;

public:
    OllamaClient(const std::string& baseUrl, const std::string& model);

    bool isAvailable() const;
};
