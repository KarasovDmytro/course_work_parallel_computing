#pragma once
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "concurrency/threadPool.h"
#include "llm/ollamaClient.h"

struct ExpansionRequest {
    std::string query;
    std::chrono::steady_clock::time_point deadline;
    std::vector<std::string> terms;
    bool done = false;
    std::mutex mutex;
    std::condition_variable finished;
};

class QueryExpander {
private:
    OllamaClient& ollama;
    int timeoutMs;
    ThreadPool worker;

    void process(std::shared_ptr<ExpansionRequest> request);

public:
    QueryExpander(OllamaClient& ollama, int timeoutSec);

    std::vector<std::string> expand(const std::string& query);
};
