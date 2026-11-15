#include "httpClient.h"
#include <curl/curl.h>

using namespace std;

static size_t saveBody(char* data, size_t size, size_t count, void* userData) {
    string* body = (string*)userData;
    body->append(data, size * count);
    return size * count;
}

HttpClient::HttpClient(const string& baseUrl) : baseUrl(baseUrl) {
}

HttpResponse HttpClient::get(const string& path, long timeoutMs) const {
    return perform(path, nullptr, timeoutMs);
}

HttpResponse HttpClient::post(const string& path, const string& body, long timeoutMs) const {
    return perform(path, &body, timeoutMs);
}

const string& HttpClient::url() const {
    return baseUrl;
}

HttpResponse HttpClient::perform(const string& path, const string* body, long timeoutMs) const {
    HttpResponse response;
    response.connected = false;
    response.status = 0;

    CURL* handle = curl_easy_init();
    if (handle == nullptr) {
        response.error = "cannot initialize libcurl";
        return response;
    }

    string url = baseUrl + path;
    curl_slist* headers = nullptr;
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, timeoutMs);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, saveBody);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response.body);

    if (body != nullptr) {
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, body->c_str());
    }

    CURLcode result = curl_easy_perform(handle);
    if (result == CURLE_OK) {
        response.connected = true;
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response.status);
    } else {
        response.error = curl_easy_strerror(result);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(handle);
    return response;
}
