#pragma once
#include <string>

using namespace std;

class Config {
public:
    int httpPort;
    string ollamaUrl;
    string ollamaModel;

    Config();
};
