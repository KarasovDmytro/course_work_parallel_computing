#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include "crow_all.h"

crow::response jsonResponse(int code, const nlohmann::json& body);
crow::response errorResponse(int code, const std::string& message);
