#include "fileReader.h"
#include <fstream>
#include <stdexcept>

using namespace std;

string readFile(const string& path) {
    ifstream file(path, ios::binary);
    if (!file) {
        throw runtime_error("cannot open " + path);
    }

    file.seekg(0, ios::end);
    long long size = file.tellg();
    if (size < 0) {
        throw runtime_error("cannot read " + path);
    }
    file.seekg(0, ios::beg);

    string text(size, '\0');
    file.read(&text[0], size);
    text.resize(file.gcount());
    return text;
}
