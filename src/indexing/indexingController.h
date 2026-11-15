#pragma once
#include "crow_all.h"
#include "indexing/indexBuilder.h"

class IndexingController {
private:
    IndexBuilder& builder;

    crow::response handleIndex(const crow::request& request);
    crow::response handleStats();

public:
    IndexingController(IndexBuilder& builder);

    void registerRoutes(crow::SimpleApp& app);
};
