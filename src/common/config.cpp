#include "config.h"
#include <cstdlib>

Config::Config() {
    httpPort = 8080;
    ollamaUrl = "http://localhost:11434";

    const char* port = getenv("HTTP_PORT");
    if (port != nullptr) {
        int value = atoi(port);
        if (value > 0) {
            httpPort = value;
        }
    }

    const char* url = getenv("OLLAMA_URL");
    if (url != nullptr) {
        ollamaUrl = url;
    }
}