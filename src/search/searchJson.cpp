#include "searchJson.h"

using namespace std;
using json = nlohmann::json;

json resultsToJson(const vector<SearchResult>& results) {
    json items = json::array();

    for (int i = 0; i < (int)results.size(); i++) {
        json item;
        item["doc_id"] = results[i].docId;
        item["score"] = results[i].score;
        item["snippet"] = results[i].snippet;
        items.push_back(item);
    }
    return items;
}
