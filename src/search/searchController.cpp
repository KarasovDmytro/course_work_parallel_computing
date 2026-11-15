#include "searchController.h"
#include <chrono>
#include <exception>
#include <nlohmann/json.hpp>
#include "common/jsonResponse.h"

using namespace std;
using json = nlohmann::json;

const int defaultLimit = 10;
const int minLimit = 1;
const int maxLimit = 100;

SearchController::SearchController(SearchService& service, QueryExpander& expander)
    : service(service), expander(expander) {
}

void SearchController::registerRoutes(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/api/search").methods(crow::HTTPMethod::Post)([this](const crow::request& request) {
        return handleSearch(request);
    });
}

crow::response SearchController::handleSearch(const crow::request& request) {
    chrono::steady_clock::time_point start = chrono::steady_clock::now();

    json body = json::parse(request.body, nullptr, false);
    if (body.is_discarded() || !body.is_object()) {
        return errorResponse(400, "request body must be a JSON object");
    }

    if (!body.contains("query") || !body["query"].is_string() || body["query"].get<string>().empty()) {
        return errorResponse(400, "query must be a non-empty string");
    }
    string query = body["query"].get<string>();

    string mode = "keyword";
    if (body.contains("mode")) {
        if (!body["mode"].is_string()) {
            return errorResponse(400, "mode must be \"keyword\" or \"expanded\"");
        }
        mode = body["mode"].get<string>();
        if (mode != "keyword" && mode != "expanded") {
            return errorResponse(400, "mode must be \"keyword\" or \"expanded\"");
        }
    }

    int limit = defaultLimit;
    if (body.contains("limit")) {
        if (!body["limit"].is_number_integer()) {
            return errorResponse(400, "limit must be an integer");
        }
        long long value = body["limit"].get<long long>();
        if (value < minLimit || value > maxLimit) {
            return errorResponse(400, "limit must be between 1 and 100");
        }
        limit = (int)value;
    }

    vector<string> expandedTerms;
    if (mode == "expanded") {
        if (!service.isReady()) {
            return errorResponse(409, "index is not ready");
        }
        expandedTerms = expander.expand(query);
    }

    SearchOutcome outcome;
    try {
        outcome = service.search(query, expandedTerms, limit);
    } catch (const exception& error) {
        return errorResponse(500, string("search failed: ") + error.what());
    }

    if (!outcome.ready) {
        return errorResponse(409, "index is not ready");
    }

    json results = json::array();
    for (int i = 0; i < (int)outcome.results.size(); i++) {
        json item;
        item["doc_id"] = outcome.results[i].docId;
        item["score"] = outcome.results[i].score;
        item["snippet"] = outcome.results[i].snippet;
        results.push_back(item);
    }

    json response;
    response["results"] = results;
    if (mode == "expanded") {
        response["expanded_terms"] = expandedTerms;
    }
    response["time_ms"] = (int)chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start).count();
    return jsonResponse(200, response);
}
