#include "jsonResponse.h"

using namespace std;
using json = nlohmann::json;

crow::response jsonResponse(int code, const json& body) {
    crow::response response(code, body.dump(-1, ' ', false, json::error_handler_t::replace));
    response.set_header("Content-Type", "application/json");
    return response;
}

crow::response errorResponse(int code, const string& message) {
    json body;
    body["status"] = "error";
    body["message"] = message;
    return jsonResponse(code, body);
}
