#include "termHash.h"

using namespace std;

const unsigned int fnvOffsetBasis = 2166136261u;
const unsigned int fnvPrime = 16777619u;

unsigned int termHash(const string& term) {
    unsigned int hash = fnvOffsetBasis;

    for (int i = 0; i < (int)term.size(); i++) {
        hash ^= (unsigned char)term[i];
        hash *= fnvPrime;
    }
    return hash;
}
