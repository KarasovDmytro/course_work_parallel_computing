#include "ollamaClient.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

size_t saveResponse(char* data, size_t size, size_t count, void* userData) {
    string* body = (string*)userData;
    body->append(data, size * count);
    return size * count;
}

OllamaClient::OllamaClient(const string& baseUrl, const string& model) {
    this->baseUrl = baseUrl;
    this->model = model;
}

bool OllamaClient::isAvailable() const {
    CURL* handle = curl_easy_init();

    if (handle == nullptr) {
        return false;
    }

    string url = baseUrl + "/api/tags";
    string body;
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, 1500L);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, saveResponse);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &body);

    CURLcode result = curl_easy_perform(handle);

    long statusCode = 0;
    if (result == CURLE_OK) {
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &statusCode);
    }

    curl_easy_cleanup(handle);

    if (result != CURLE_OK || statusCode != 200) {
        return false;
    }
    return hasModel(body);
}

bool OllamaClient::hasModel(const string& tagsJson) const {
    json tags = json::parse(tagsJson, nullptr, false);

    if (tags.is_discarded() || !tags.contains("models") || !tags["models"].is_array()) {
        return false;
    }

    string wanted = model;
    if (wanted.find(':') == string::npos) {
        wanted += ":latest";
    }

    for (const json& item : tags["models"]) {
        if (item.value("name", "") == wanted) {
            return true;
        }
    }
    return false;
}
