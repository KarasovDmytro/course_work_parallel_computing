#include "indexingController.h"
#include <exception>
#include <thread>
#include <nlohmann/json.hpp>
#include "common/jsonResponse.h"

using namespace std;
using json = nlohmann::json;

const int minThreads = 1;
const int maxThreads = 256;

IndexingController::IndexingController(IndexBuilder& builder) : builder(builder) {
}

void IndexingController::registerRoutes(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/api/index").methods(crow::HTTPMethod::Post)([this](const crow::request& request) {
        return handleIndex(request);
    });

    CROW_ROUTE(app, "/api/stats").methods(crow::HTTPMethod::Get)([this]() {
        return handleStats();
    });
}

crow::response IndexingController::handleIndex(const crow::request& request) {
    int threadCount = (int)thread::hardware_concurrency();
    if (threadCount < minThreads) {
        threadCount = minThreads;
    }

    if (!request.body.empty()) {
        json body = json::parse(request.body, nullptr, false);
        if (body.is_discarded() || !body.is_object()) {
            return errorResponse(400, "request body must be a JSON object");
        }

        if (body.contains("threads")) {
            if (!body["threads"].is_number_integer()) {
                return errorResponse(400, "threads must be an integer");
            }
            long long value = body["threads"].get<long long>();
            if (value < minThreads || value > maxThreads) {
                return errorResponse(400, "threads must be between 1 and 256");
            }
            threadCount = (int)value;
        }
    }

    BuildResult result;
    try {
        result = builder.build(threadCount);
    } catch (const exception& error) {
        return errorResponse(500, string("indexing failed: ") + error.what());
    }

    if (!result.started) {
        return errorResponse(409, "indexing is already in progress");
    }

    json response;
    response["status"] = "ok";
    response["documents_indexed"] = result.documents;
    response["time_ms"] = result.timeMs;
    return jsonResponse(200, response);
}

crow::response IndexingController::handleStats() {
    BuildStats stats = builder.stats();

    json response;
    response["documents"] = stats.documents;
    response["index_build_time_ms"] = stats.buildTimeMs;
    response["threads_used"] = stats.threadsUsed;
    return jsonResponse(200, response);
}
