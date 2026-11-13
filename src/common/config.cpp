#include "config.h"
#include <cstdlib>

using namespace std;

Config::Config() {
    httpPort = 8080;
    ollamaUrl = "http://localhost:11434";
    ollamaModel = "qwen2.5:0.5b";

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

    const char* model = getenv("OLLAMA_MODEL");
    if (model != nullptr && model[0] != '\0') {
        ollamaModel = model;
    }
}
