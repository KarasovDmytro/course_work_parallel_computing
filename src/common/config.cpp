#include "config.h"
#include <cstdlib>

using namespace std;

Config::Config() {
    httpPort = 8080;
    httpThreads = 64;
    ollamaUrl = "http://localhost:11434";
    ollamaModel = "qwen2.5:0.5b";
    dataDir = "./data";
    llmTimeoutSec = 30;

    const char* port = getenv("HTTP_PORT");
    if (port != nullptr) {
        int value = atoi(port);
        if (value > 0) {
            httpPort = value;
        }
    }

    const char* threads = getenv("HTTP_THREADS");
    if (threads != nullptr) {
        int value = atoi(threads);
        if (value > 0) {
            httpThreads = value;
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

    const char* data = getenv("DATA_DIR");
    if (data != nullptr && data[0] != '\0') {
        dataDir = data;
    }

    const char* timeout = getenv("LLM_TIMEOUT_SEC");
    if (timeout != nullptr) {
        int value = atoi(timeout);
        if (value > 0) {
            llmTimeoutSec = value;
        }
    }
}
