#pragma once
#include "crow_all.h"
#include "search/searchService.h"

class SearchController {
private:
    SearchService& service;

    crow::response handleSearch(const crow::request& request);

public:
    SearchController(SearchService& service);

    void registerRoutes(crow::SimpleApp& app);
};
