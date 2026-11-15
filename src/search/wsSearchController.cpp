#include "wsSearchController.h"
#include <chrono>
#include <exception>
#include "search/searchJson.h"

using namespace std;
using json = nlohmann::json;

const int wsResultLimit = 10;

WsSearchController::WsSearchController(SearchService& service, QueryExpander& expander, int workerCount)
    : service(service), expander(expander), workers(workerCount) {
}

void WsSearchController::registerRoutes(crow::SimpleApp& app) {
    CROW_WEBSOCKET_ROUTE(app, "/ws/search")
        .onopen([this](crow::websocket::connection& connection) {
            openSession(connection);
        })
        .onclose([this](crow::websocket::connection& connection, const string& reason, uint16_t code) {
            closeSession(connection);
        })
        .onmessage([this](crow::websocket::connection& connection, const string& data, bool isBinary) {
            handleMessage(connection, data);
        });
}

void WsSearchController::openSession(crow::websocket::connection& connection) {
    shared_ptr<WsSession> session = make_shared<WsSession>();
    session->connection = &connection;

    lock_guard<mutex> lock(sessionsMutex);
    sessions[&connection] = session;
}

void WsSearchController::closeSession(crow::websocket::connection& connection) {
    shared_ptr<WsSession> session;
    {
        lock_guard<mutex> lock(sessionsMutex);
        map<crow::websocket::connection*, shared_ptr<WsSession>>::iterator it = sessions.find(&connection);
        if (it == sessions.end()) {
            return;
        }
        session = it->second;
        sessions.erase(it);
    }

    lock_guard<mutex> lock(session->mutex);
    session->open = false;
}

shared_ptr<WsSession> WsSearchController::findSession(crow::websocket::connection& connection) {
    lock_guard<mutex> lock(sessionsMutex);
    map<crow::websocket::connection*, shared_ptr<WsSession>>::iterator it = sessions.find(&connection);
    if (it == sessions.end()) {
        return nullptr;
    }
    return it->second;
}

void WsSearchController::handleMessage(crow::websocket::connection& connection, const string& data) {
    shared_ptr<WsSession> session = findSession(connection);
    if (session == nullptr) {
        return;
    }

    json body = json::parse(data, nullptr, false);
    if (body.is_discarded() || !body.is_object()) {
        sendError(session, "message must be a JSON object");
        return;
    }
    if (!body.contains("query") || !body["query"].is_string() || body["query"].get<string>().empty()) {
        sendError(session, "query must be a non-empty string");
        return;
    }
    string query = body["query"].get<string>();

    bool accepted = false;
    {
        lock_guard<mutex> lock(session->mutex);
        if (!session->busy) {
            session->busy = true;
            accepted = true;
        }
    }
    if (!accepted) {
        sendError(session, "previous query is still in progress");
        return;
    }

    bool submitted = workers.submit([this, session, query] {
        processQuery(session, query);
    });
    if (!submitted) {
        lock_guard<mutex> lock(session->mutex);
        session->busy = false;
    }
}

void WsSearchController::processQuery(shared_ptr<WsSession> session, const string& query) {
    try {
        if (!service.isReady()) {
            sendError(session, "index is not ready");
        } else {
            vector<string> expandedTerms = expander.expand(query);

            json expansion;
            expansion["stage"] = "query_expansion";
            expansion["data"]["original"] = query;
            expansion["data"]["expanded_terms"] = expandedTerms;
            send(session, expansion);

            chrono::steady_clock::time_point start = chrono::steady_clock::now();
            SearchOutcome outcome = service.search(query, expandedTerms, wsResultLimit);
            int timeMs = (int)chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start).count();

            if (!outcome.ready) {
                sendError(session, "index is not ready");
            } else {
                json results;
                results["stage"] = "keyword_search";
                results["data"]["results"] = resultsToJson(outcome.results);
                results["data"]["time_ms"] = timeMs;
                send(session, results);

                json done;
                done["stage"] = "done";
                send(session, done);
            }
        }
    } catch (const exception& error) {
        sendError(session, string("search failed: ") + error.what());
    }

    lock_guard<mutex> lock(session->mutex);
    session->busy = false;
}

void WsSearchController::send(const shared_ptr<WsSession>& session, const json& message) {
    string text = message.dump(-1, ' ', false, json::error_handler_t::replace);

    lock_guard<mutex> lock(session->mutex);
    if (session->open) {
        session->connection->send_text(text);
    }
}

void WsSearchController::sendError(const shared_ptr<WsSession>& session, const string& text) {
    json message;
    message["stage"] = "error";
    message["data"]["message"] = text;
    send(session, message);
}
