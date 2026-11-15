#pragma once
#include <string>
#include "crow_all.h"

class WebController {
private:
    std::string webDir;

    crow::response handleIndex();

public:
    WebController(const std::string& webDir);

    void registerRoutes(crow::SimpleApp& app);
};
