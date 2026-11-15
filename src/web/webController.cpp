#include "webController.h"
#include <exception>
#include "common/fileReader.h"
#include "common/jsonResponse.h"

using namespace std;

WebController::WebController(const string& webDir) : webDir(webDir) {
}

void WebController::registerRoutes(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/").methods(crow::HTTPMethod::Get)([this]() {
        return handleIndex();
    });
}

crow::response WebController::handleIndex() {
    string page;
    try {
        page = readFile(webDir + "/index.html");
    } catch (const exception& error) {
        return errorResponse(500, "web page is not available");
    }

    crow::response response(200, page);
    response.set_header("Content-Type", "text/html; charset=utf-8");
    return response;
}
