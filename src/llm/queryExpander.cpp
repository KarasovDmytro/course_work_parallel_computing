#include "queryExpander.h"

using namespace std;

const int minCallMs = 200;

QueryExpander::QueryExpander(OllamaClient& ollama, int timeoutSec)
    : ollama(ollama), timeoutMs(timeoutSec * 1000), worker(1) {
}

vector<string> QueryExpander::expand(const string& query) {
    shared_ptr<ExpansionRequest> request = make_shared<ExpansionRequest>();
    request->query = query;
    request->deadline = chrono::steady_clock::now() + chrono::milliseconds(timeoutMs);

    if (!worker.submit([this, request] { process(request); })) {
        return {};
    }

    unique_lock<mutex> lock(request->mutex);
    bool done = request->finished.wait_until(lock, request->deadline, [&request] {
        return request->done;
    });

    if (!done) {
        return {};
    }
    return request->terms;
}

void QueryExpander::process(shared_ptr<ExpansionRequest> request) {
    chrono::steady_clock::duration left = request->deadline - chrono::steady_clock::now();
    int remainingMs = (int)chrono::duration_cast<chrono::milliseconds>(left).count();

    vector<string> terms;
    if (remainingMs >= minCallMs) {
        terms = ollama.expandQuery(request->query, remainingMs);
    }

    {
        lock_guard<mutex> lock(request->mutex);
        request->terms = terms;
        request->done = true;
    }
    request->finished.notify_one();
}
