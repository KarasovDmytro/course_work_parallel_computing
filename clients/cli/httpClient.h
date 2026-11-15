#pragma once
#include <string>

struct HttpResponse {
    bool connected;
    long status;
    std::string body;
    std::string error;
};

class HttpClient {
private:
    std::string baseUrl;

    HttpResponse perform(const std::string& path, const std::string* body, long timeoutMs) const;

public:
    HttpClient(const std::string& baseUrl);

    HttpResponse get(const std::string& path, long timeoutMs) const;
    HttpResponse post(const std::string& path, const std::string& body, long timeoutMs) const;
    const std::string& url() const;
};
