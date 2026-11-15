#include "ollamaClient.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using namespace std;

using json = nlohmann::json;

const int maxExpandedTerms = 6;
const int maxGeneratedTokens = 64;

const string expansionPrompt =
    "Give synonyms and related words for a search query. "
    "Use single English words and do not repeat the query words.\n"
    "Query: sad child\n"
    "{\"terms\": [\"unhappy\", \"sorrow\", \"kid\", \"boy\", \"girl\"]}\n"
    "Query: big house\n"
    "{\"terms\": [\"large\", \"huge\", \"home\", \"mansion\", \"building\"]}\n"
    "Query: ";

size_t saveResponse(char* data, size_t size, size_t count, void* userData) {
    string* body = (string*)userData;
    body->append(data, size * count);
    return size * count;
}

OllamaClient::OllamaClient(const string& baseUrl, const string& model) {
    this->baseUrl = baseUrl;
    this->model = model;
}

bool OllamaClient::isAvailable() const {
    CURL* handle = curl_easy_init();

    if (handle == nullptr) {
        return false;
    }

    string url = baseUrl + "/api/tags";
    string body;
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, 1500L);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, saveResponse);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &body);

    CURLcode result = curl_easy_perform(handle);

    long statusCode = 0;
    if (result == CURLE_OK) {
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &statusCode);
    }

    curl_easy_cleanup(handle);

    if (result != CURLE_OK || statusCode != 200) {
        return false;
    }
    return hasModel(body);
}

bool OllamaClient::hasModel(const string& tagsJson) const {
    json tags = json::parse(tagsJson, nullptr, false);

    if (tags.is_discarded() || !tags.contains("models") || !tags["models"].is_array()) {
        return false;
    }

    string wanted = model;
    if (wanted.find(':') == string::npos) {
        wanted += ":latest";
    }

    for (const json& item : tags["models"]) {
        if (item.value("name", "") == wanted) {
            return true;
        }
    }
    return false;
}

vector<string> OllamaClient::expandQuery(const string& query, int timeoutMs) const {
    if (timeoutMs <= 0) {
        return {};
    }

    CURL* handle = curl_easy_init();
    if (handle == nullptr) {
        return {};
    }

    json request;
    request["model"] = model;
    request["prompt"] = expansionPrompt + query + "\n";
    request["stream"] = false;
    request["format"]["type"] = "object";
    request["format"]["properties"]["terms"]["type"] = "array";
    request["format"]["properties"]["terms"]["items"]["type"] = "string";
    request["format"]["properties"]["terms"]["maxItems"] = maxExpandedTerms;
    request["format"]["required"] = json::array({"terms"});
    request["options"]["temperature"] = 0;
    request["options"]["num_predict"] = maxGeneratedTokens;
    string requestBody = request.dump(-1, ' ', false, json::error_handler_t::replace);

    string url = baseUrl + "/api/generate";
    string body;
    curl_slist* headers = curl_slist_append(nullptr, "Content-Type: application/json");
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, requestBody.c_str());
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, (long)timeoutMs);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, saveResponse);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &body);

    CURLcode result = curl_easy_perform(handle);

    long statusCode = 0;
    if (result == CURLE_OK) {
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &statusCode);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(handle);

    if (result != CURLE_OK || statusCode != 200) {
        return {};
    }
    return parseTerms(body);
}

vector<string> OllamaClient::parseTerms(const string& generateJson) const {
    json reply = json::parse(generateJson, nullptr, false);
    if (reply.is_discarded() || !reply.contains("response") || !reply["response"].is_string()) {
        return {};
    }

    json answer = json::parse(reply["response"].get<string>(), nullptr, false);
    if (answer.is_discarded() || !answer.is_object() || !answer.contains("terms") || !answer["terms"].is_array()) {
        return {};
    }

    vector<string> terms;
    for (const json& item : answer["terms"]) {
        if (!item.is_string() || item.get<string>().empty()) {
            continue;
        }

        string term = item.get<string>();
        bool repeated = false;
        for (int i = 0; i < (int)terms.size(); i++) {
            if (terms[i] == term) {
                repeated = true;
                break;
            }
        }

        if (!repeated) {
            terms.push_back(term);
        }
        if ((int)terms.size() == maxExpandedTerms) {
            break;
        }
    }
    return terms;
}
