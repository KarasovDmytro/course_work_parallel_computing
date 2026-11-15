#pragma once
#include <string>

class Config {
public:
    int httpPort;
    int httpThreads;
    std::string ollamaUrl;
    std::string ollamaModel;
    std::string dataDir;
    std::string webDir;
    int llmTimeoutSec;

    Config();
};
