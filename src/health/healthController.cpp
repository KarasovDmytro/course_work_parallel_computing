#include "healthController.h"

HealthController::HealthController(OllamaClient& ollama) : ollama(ollama) {}

void HealthController::registerRoutes(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/api/health")([this]() {
        string body = "{\"status\":\"ok\",\"ollama\":";

        if (ollama.isAvailable()) {
            body += "true";
        } else {
            body += "false";
        }
        body += "}";

        crow::response res(200, body);
        res.set_header("Content-Type", "application/json");
        return res;
    });
}