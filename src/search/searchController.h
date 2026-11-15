#pragma once
#include "crow_all.h"
#include "llm/queryExpander.h"
#include "search/searchService.h"

class SearchController {
private:
    SearchService& service;
    QueryExpander& expander;

    crow::response handleSearch(const crow::request& request);

public:
    SearchController(SearchService& service, QueryExpander& expander);

    void registerRoutes(crow::SimpleApp& app);
};
