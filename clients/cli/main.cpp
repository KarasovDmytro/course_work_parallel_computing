#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "httpClient.h"

using namespace std;
using json = nlohmann::json;

const long healthTimeoutMs = 5000;
const long statsTimeoutMs = 10000;
const long searchTimeoutMs = 120000;
const long noTimeout = 0;

void printUsage() {
    cout << "Usage:\n"
         << "  search <query words...> [--mode keyword|expanded] [--limit N]\n"
         << "  search index [--threads N]\n"
         << "  search stats\n"
         << "  search health\n"
         << "\n"
         << "Use -- before the query to search for the words index, stats or health.\n"
         << "Server address: SEARCH_URL (default http://localhost:$HTTP_PORT, port 8080).\n";
}

string serverUrl() {
    const char* url = getenv("SEARCH_URL");
    if (url != nullptr && url[0] != '\0') {
        return url;
    }

    const char* port = getenv("HTTP_PORT");
    if (port != nullptr && port[0] != '\0') {
        return string("http://localhost:") + port;
    }
    return "http://localhost:8080";
}

bool parseNumber(const string& text, int& value) {
    if (text.empty()) {
        return false;
    }
    for (int i = 0; i < (int)text.size(); i++) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
    }
    if (text.size() > 9) {
        return false;
    }
    value = atoi(text.c_str());
    return true;
}

int reportFailure(const HttpClient& client, const HttpResponse& response) {
    if (!response.connected) {
        cerr << "Error: cannot connect to the server at " << client.url() << " (" << response.error << ")\n";
        return 2;
    }

    json body = json::parse(response.body, nullptr, false);
    string message = "unexpected response";
    if (!body.is_discarded() && body.is_object() && body.contains("message") && body["message"].is_string()) {
        message = body["message"].get<string>();
    }
    cerr << "Error (" << response.status << "): " << message << "\n";
    return 1;
}

json parseBody(const HttpResponse& response) {
    json body = json::parse(response.body, nullptr, false);
    if (body.is_discarded() || !body.is_object()) {
        return json::object();
    }
    return body;
}

int runHealth(const HttpClient& client) {
    HttpResponse response = client.get("/api/health", healthTimeoutMs);
    if (!response.connected || response.status != 200) {
        return reportFailure(client, response);
    }

    json body = parseBody(response);
    cout << "Server: " << body.value("status", "unknown") << "\n";
    cout << "Ollama: " << (body.value("ollama", false) ? "available" : "unavailable") << "\n";
    return 0;
}

int runStats(const HttpClient& client) {
    HttpResponse response = client.get("/api/stats", statsTimeoutMs);
    if (!response.connected || response.status != 200) {
        return reportFailure(client, response);
    }

    json body = parseBody(response);
    cout << "Documents:    " << body.value("documents", 0) << "\n";
    cout << "Build time:   " << body.value("index_build_time_ms", 0) << " ms\n";
    cout << "Threads used: " << body.value("threads_used", 0) << "\n";
    return 0;
}

int runIndex(const HttpClient& client, const vector<string>& args) {
    json request = json::object();

    for (int i = 1; i < (int)args.size(); i++) {
        int threads = 0;
        if (args[i] == "--threads" && i + 1 < (int)args.size() && parseNumber(args[i + 1], threads)) {
            request["threads"] = threads;
            i++;
        } else {
            cerr << "Error: expected --threads N\n";
            return 1;
        }
    }

    cout << "Building the index..." << endl;
    HttpResponse response = client.post("/api/index", request.dump(), noTimeout);
    if (!response.connected || response.status != 200) {
        return reportFailure(client, response);
    }

    json body = parseBody(response);
    cout << "Indexed " << body.value("documents_indexed", 0) << " documents in " << body.value("time_ms", 0) << " ms\n";
    return 0;
}

int runSearch(const HttpClient& client, const vector<string>& args) {
    string query;
    json request = json::object();
    bool onlyWords = false;

    for (int i = 0; i < (int)args.size(); i++) {
        int limit = 0;
        if (!onlyWords && args[i] == "--") {
            onlyWords = true;
        } else if (!onlyWords && args[i] == "--mode" && i + 1 < (int)args.size()) {
            request["mode"] = args[i + 1];
            i++;
        } else if (!onlyWords && args[i] == "--limit" && i + 1 < (int)args.size() && parseNumber(args[i + 1], limit)) {
            request["limit"] = limit;
            i++;
        } else if (!onlyWords && args[i].size() > 1 && args[i][0] == '-' && args[i][1] == '-') {
            cerr << "Error: unknown or incomplete option " << args[i] << "\n";
            return 1;
        } else {
            if (!query.empty()) {
                query += ' ';
            }
            query += args[i];
        }
    }

    if (query.empty()) {
        cerr << "Error: the query is empty\n";
        return 1;
    }
    request["query"] = query;

    HttpResponse response = client.post("/api/search", request.dump(), searchTimeoutMs);
    if (!response.connected || response.status != 200) {
        return reportFailure(client, response);
    }

    json body = parseBody(response);
    json results = body.value("results", json::array());
    cout << "Query: " << query << "\n";

    if (body.contains("expanded_terms") && body["expanded_terms"].is_array()) {
        cout << "Expanded terms:";
        if (body["expanded_terms"].empty()) {
            cout << " (none)";
        }
        for (int i = 0; i < (int)body["expanded_terms"].size(); i++) {
            cout << (i == 0 ? " " : ", ") << body["expanded_terms"][i].get<string>();
        }
        cout << "\n";
    }
    cout << "Found " << results.size() << " results in " << body.value("time_ms", 0) << " ms\n";

    for (int i = 0; i < (int)results.size(); i++) {
        cout << "\n" << (i + 1) << ". " << results[i].value("doc_id", "")
             << "  (score " << fixed << setprecision(3) << results[i].value("score", 0.0) << ")\n";
        cout << "   " << results[i].value("snippet", "") << "\n";
    }
    return 0;
}

int main(int argc, char** argv) {
    vector<string> args;
    for (int i = 1; i < argc; i++) {
        args.push_back(argv[i]);
    }

    if (args.empty()) {
        printUsage();
        return 1;
    }
    if (args[0] == "-h" || args[0] == "--help") {
        printUsage();
        return 0;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    HttpClient client(serverUrl());

    int code;
    if (args[0] == "health" && args.size() == 1) {
        code = runHealth(client);
    } else if (args[0] == "stats" && args.size() == 1) {
        code = runStats(client);
    } else if (args[0] == "index") {
        code = runIndex(client, args);
    } else {
        code = runSearch(client, args);
    }

    curl_global_cleanup();
    return code;
}
