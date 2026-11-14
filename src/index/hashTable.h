#pragma once
#include <string>
#include <utility>
#include <vector>

template <typename Value>
struct HashEntry {
    std::string key;
    unsigned int hash = 0;
    bool used = false;
    Value value{};
};

template <typename Value>
class HashTable {
private:
    std::vector<HashEntry<Value>> slots;
    int count = 0;

    int findSlot(const std::string& key, unsigned int hash) const;
    void grow();

public:
    HashTable(int initialCapacity = 16);

    Value& getOrCreate(const std::string& key, unsigned int hash);
    const Value* find(const std::string& key, unsigned int hash) const;

    int size() const;
    int capacity() const;
    const HashEntry<Value>& slot(int i) const;
};

template <typename Value>
HashTable<Value>::HashTable(int initialCapacity) {
    int capacity = 16;
    while (capacity < initialCapacity) {
        capacity *= 2;
    }
    slots.resize(capacity);
}

template <typename Value>
int HashTable<Value>::findSlot(const std::string& key, unsigned int hash) const {
    int mask = (int)slots.size() - 1;
    int i = hash & mask;

    while (slots[i].used) {
        if (slots[i].hash == hash && slots[i].key == key) {
            return i;
        }
        i = (i + 1) & mask;
    }
    return i;
}

template <typename Value>
void HashTable<Value>::grow() {
    std::vector<HashEntry<Value>> oldSlots = std::move(slots);
    slots.clear();
    slots.resize(oldSlots.size() * 2);
    int mask = (int)slots.size() - 1;

    for (int j = 0; j < (int)oldSlots.size(); j++) {
        if (!oldSlots[j].used) {
            continue;
        }

        int i = oldSlots[j].hash & mask;
        while (slots[i].used) {
            i = (i + 1) & mask;
        }
        slots[i] = std::move(oldSlots[j]);
    }
}

template <typename Value>
Value& HashTable<Value>::getOrCreate(const std::string& key, unsigned int hash) {
    int i = findSlot(key, hash);
    if (slots[i].used) {
        return slots[i].value;
    }

    if ((count + 1) * 10 > (int)slots.size() * 7) {
        grow();
        i = findSlot(key, hash);
    }

    slots[i].key = key;
    slots[i].hash = hash;
    slots[i].used = true;
    count++;
    return slots[i].value;
}

template <typename Value>
const Value* HashTable<Value>::find(const std::string& key, unsigned int hash) const {
    int i = findSlot(key, hash);
    if (!slots[i].used) {
        return nullptr;
    }
    return &slots[i].value;
}

template <typename Value>
int HashTable<Value>::size() const {
    return count;
}

template <typename Value>
int HashTable<Value>::capacity() const {
    return (int)slots.size();
}

template <typename Value>
const HashEntry<Value>& HashTable<Value>::slot(int i) const {
    return slots[i];
}
