#include <iostream>
#include "crow_all.h"

using namespace std;

int main() {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/api/health")([]() {
        crow::response res(200, "{\"status\":\"ok\",\"ollama\":false}");
        res.set_header("Content-Type", "application/json");
        return res;
    });

    cout << "Server started on port 8080" << endl;
    app.port(8080).multithreaded().run();

    return 0;
}
