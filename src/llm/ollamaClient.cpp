#include "ollamaClient.h"
#include <curl/curl.h>

size_t ignoreResponse(char* data, size_t size, size_t count, void* userData) {
    return size * count;
}

OllamaClient::OllamaClient(const string& baseUrl) {
    this->baseUrl = baseUrl;
}

bool OllamaClient::isAvailable() const {
    CURL* handle = curl_easy_init();

    if (handle == nullptr) {
        return false;
    }

    string url = baseUrl + "/api/tags";
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, 1500L);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, ignoreResponse);

    CURLcode result = curl_easy_perform(handle);

    long statusCode = 0;
    if (result == CURLE_OK) {
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &statusCode);
    }

    curl_easy_cleanup(handle);
    return result == CURLE_OK && statusCode == 200;
}