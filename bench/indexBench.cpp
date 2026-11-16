#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include "index/invertedIndex.h"
#include "index/termHash.h"
#include "indexing/indexBuilder.h"
#include "text/tokenizer.h"

using namespace std;

struct BenchSettings {
    string dataDir = "./data";
    int threads = 1;
    int shards = 64;
    int warmup = 1;
    int repeat = 5;
    BuildOptions options;
    vector<string> queries;
};

void printUsage() {
    cout << "Usage: index_bench [options]\n"
         << "  --data DIR         dataset folder (default DATA_DIR or ./data)\n"
         << "  --threads N        index threads (default: number of logical cores)\n"
         << "  --shards K         index shards, rounded up to a power of two (default 64)\n"
         << "  --per-token-lock   write every token into the shard under its lock (E6)\n"
         << "  --as-is-order      submit files in path order instead of largest first (E7)\n"
         << "  --no-stemming      build the index without the Porter stemmer (E8)\n"
         << "  --warmup W         builds before measuring, not printed (default 1)\n"
         << "  --repeat R         measured builds (default 5)\n"
         << "  --query TEXT       count documents that match TEXT; can be repeated\n"
         << "Prints one CSV row per measured build.\n";
}

bool parseNumber(const string& text, int& value) {
    if (text.empty() || text.size() > 9) {
        return false;
    }
    for (int i = 0; i < (int)text.size(); i++) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
    }
    value = atoi(text.c_str());
    return true;
}

bool parseArguments(int argc, char* argv[], BenchSettings& settings) {
    const char* dataDir = getenv("DATA_DIR");
    if (dataDir != nullptr && dataDir[0] != '\0') {
        settings.dataDir = dataDir;
    }
    settings.threads = (int)thread::hardware_concurrency();
    if (settings.threads < 1) {
        settings.threads = 1;
    }

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        bool hasValue = i + 1 < argc;

        if (arg == "--per-token-lock") {
            settings.options.perTokenLock = true;
        } else if (arg == "--as-is-order") {
            settings.options.largestFirst = false;
        } else if (arg == "--no-stemming") {
            settings.options.useStemming = false;
        } else if (arg == "--data" && hasValue) {
            settings.dataDir = argv[++i];
        } else if (arg == "--query" && hasValue) {
            settings.queries.push_back(argv[++i]);
        } else if (arg == "--threads" && hasValue) {
            if (!parseNumber(argv[++i], settings.threads) || settings.threads < 1) {
                return false;
            }
        } else if (arg == "--shards" && hasValue) {
            if (!parseNumber(argv[++i], settings.shards) || settings.shards < 1) {
                return false;
            }
        } else if (arg == "--warmup" && hasValue) {
            if (!parseNumber(argv[++i], settings.warmup)) {
                return false;
            }
        } else if (arg == "--repeat" && hasValue) {
            if (!parseNumber(argv[++i], settings.repeat) || settings.repeat < 1) {
                return false;
            }
        } else {
            return false;
        }
    }
    return true;
}

int countMatches(const InvertedIndex& index, const Tokenizer& tokenizer, const string& query) {
    vector<bool> matched(index.documentCount(), false);
    int count = 0;

    vector<string> terms = tokenizer.tokenize(query);
    for (int i = 0; i < (int)terms.size(); i++) {
        const PostingList* list = index.find(terms[i], termHash(terms[i]));
        if (list == nullptr) {
            continue;
        }
        for (int j = 0; j < list->size(); j++) {
            int docId = list->get(j).docId;
            if (!matched[docId]) {
                matched[docId] = true;
                count++;
            }
        }
    }
    return count;
}

int main(int argc, char* argv[]) {
    BenchSettings settings;
    if (!parseArguments(argc, argv, settings)) {
        printUsage();
        return 1;
    }

    InvertedIndex index(settings.shards);
    IndexBuilder builder(index, settings.dataDir, settings.options);
    Tokenizer tokenizer(settings.options.useStemming);

    cout << "run,threads,shards,per_token_lock,largest_first,stemming,documents,time_ms,task_time_ms,idle,terms,locks";
    for (int q = 0; q < (int)settings.queries.size(); q++) {
        cout << ",\"hits:" << settings.queries[q] << "\"";
    }
    cout << endl;

    for (int run = 1 - settings.warmup; run <= settings.repeat; run++) {
        BuildResult result = builder.build(settings.threads);
        if (run < 1) {
            continue;
        }

        double idle = 0;
        if (result.timeMs > 0) {
            idle = 1.0 - (double)result.taskTimeMs / ((double)settings.threads * result.timeMs);
        }

        cout << run << "," << settings.threads << "," << index.shardCount() << ","
             << settings.options.perTokenLock << "," << settings.options.largestFirst << ","
             << settings.options.useStemming << "," << result.documents << ","
             << result.timeMs << "," << result.taskTimeMs << "," << idle << ","
             << index.termCount() << "," << index.lockCount();

        for (int q = 0; q < (int)settings.queries.size(); q++) {
            if (settings.options.perTokenLock) {
                cout << ",";
            } else {
                cout << "," << countMatches(index, tokenizer, settings.queries[q]);
            }
        }
        cout << endl;
    }
    return 0;
}
