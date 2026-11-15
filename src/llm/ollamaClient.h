#pragma once
#include <string>
#include <vector>

class OllamaClient {
private:
    std::string baseUrl;
    std::string model;

    bool hasModel(const std::string& tagsJson) const;
    std::vector<std::string> parseTerms(const std::string& generateJson) const;

public:
    OllamaClient(const std::string& baseUrl, const std::string& model);

    bool isAvailable() const;
    std::vector<std::string> expandQuery(const std::string& query, int timeoutMs) const;
};
