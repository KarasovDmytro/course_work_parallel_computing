#include <iostream>
#include <curl/curl.h>
#include "crow_all.h"
#include "common/config.h"
#include "llm/ollamaClient.h"
#include "health/healthController.h"

using namespace std;

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    Config config;
    OllamaClient ollama(config.ollamaUrl);
    HealthController health(ollama);

    crow::SimpleApp app;
    health.registerRoutes(app);

    cout << "Server started on port " << config.httpPort << endl;
    app.port(config.httpPort).multithreaded().run();

    curl_global_cleanup();
    return 0;
}
