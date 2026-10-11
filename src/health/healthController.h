#pragma once
#include "crow_all.h"
#include "llm/ollamaClient.h"

using namespace std;

class HealthController {
private:
    OllamaClient& ollama;

public:
    HealthController(OllamaClient& ollama);

    void registerRoutes(crow::SimpleApp& app);
};
