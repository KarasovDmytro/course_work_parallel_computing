#pragma once
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <nlohmann/json.hpp>
#include "crow_all.h"
#include "concurrency/threadPool.h"
#include "llm/queryExpander.h"
#include "search/searchService.h"

struct WsSession {
    crow::websocket::connection* connection;
    std::mutex mutex;
    bool open = true;
    bool busy = false;
};

class WsSearchController {
private:
    SearchService& service;
    QueryExpander& expander;

    std::map<crow::websocket::connection*, std::shared_ptr<WsSession>> sessions;
    std::mutex sessionsMutex;

    ThreadPool workers;

    void openSession(crow::websocket::connection& connection);
    void closeSession(crow::websocket::connection& connection);
    std::shared_ptr<WsSession> findSession(crow::websocket::connection& connection);

    void handleMessage(crow::websocket::connection& connection, const std::string& data);
    void processQuery(std::shared_ptr<WsSession> session, const std::string& query);

    void send(const std::shared_ptr<WsSession>& session, const nlohmann::json& message);
    void sendError(const std::shared_ptr<WsSession>& session, const std::string& text);

public:
    WsSearchController(SearchService& service, QueryExpander& expander, int workerCount);

    void registerRoutes(crow::SimpleApp& app);
};
