#pragma once
#include <vector>
#include <nlohmann/json.hpp>
#include "search/searchService.h"

nlohmann::json resultsToJson(const std::vector<SearchResult>& results);
