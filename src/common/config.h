#pragma once
#include <string>

class Config {
public:
    int httpPort;
    int httpThreads;
    std::string ollamaUrl;
    std::string ollamaModel;
    std::string dataDir;
    int llmTimeoutSec;

    Config();
};
