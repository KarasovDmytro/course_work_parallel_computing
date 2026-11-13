#pragma once
#include <string>

class Config {
public:
    int httpPort;
    std::string ollamaUrl;
    std::string ollamaModel;

    Config();
};
