#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include <unordered_map>
#include <list>
#include <utility>
#include <iostream>

#include "df/tiletype.h"

#include "modules/Maps.h"
#include "modules/Textures.h"

using namespace DFHack;

struct LRUCacheBlock {
    uint8_t max_depth;
    df::tiletype tiletype[16][16];
    uint16_t color[16][16];
    uint8_t depth[16][16];
    TexposHandle screentexpos_background[16][16];
    TexposHandle screentexpos_background_two[16][16];
    bool only_depthed;

    LRUCacheBlock() {
        max_depth = 0;
        only_depthed = true;
        for (int x = 0; x < 16; ++x) {
            for (int y = 0; y < 16; ++y) {
                tiletype[x][y] = (df::tiletype)65535; //df::tiletype::Void; // default
                color[x][y] = (uint16_t)65535; // default
                depth[x][y] = 8;
                screentexpos_background[x][y] = TexposHandle();
                screentexpos_background_two[x][y] = TexposHandle();
            }
        }
    }
};


template<typename Key, typename Value>
class LRUCache {
public:
    using KeyValuePair = std::pair<Key, Value>;
    using ListIterator = typename std::list<KeyValuePair>::iterator;
    int engravings_count;
    cuboid* engravings_cuboid;

    explicit LRUCache(size_t capacity) : capacity(capacity) {
        engravings_count = 0;
        engravings_cuboid = new cuboid();
    }

    ~LRUCache(){
        delete engravings_cuboid;
    }

    bool get(const Key& key, Value& value) {
        auto it = cacheItemsMap.find(key);
        if (it == cacheItemsMap.end() || it->second->second->only_depthed)
            return false;

        // Move accessed item to the front of the list
        cacheItemsList.splice(cacheItemsList.begin(), cacheItemsList, it->second);
        value = it->second->second;
        return true;
    }

    void put(const Key& key, const Value& value) {
        auto it = cacheItemsMap.find(key);
        if (it != cacheItemsMap.end()) {
            // Update value and move to front
            it->second->second = value;
            cacheItemsList.splice(cacheItemsList.begin(), cacheItemsList, it->second);
            return;
        }

        // Evict the least recently used item if capacity is exceeded
        if (cacheItemsList.size() >= capacity) {
            auto last = cacheItemsList.end();
            --last;
            cacheItemsMap.erase(last->first);
            cacheItemsList.pop_back();
        }

        // Insert new item at the front
        cacheItemsList.emplace_front(key, value);
        cacheItemsMap[key] = cacheItemsList.begin();
    }

    bool exists(const Key& key) const {
        return cacheItemsMap.find(key) != cacheItemsMap.end();
    }

    void clear() {
        cacheItemsMap.clear();
        cacheItemsList.clear();
    }

    void debugPrint() const {
        std::cout << "Cache contents:\n";
        for (const auto& [k, v] : cacheItemsList) {
            std::cout << "  " << k << " => " << v << "\n";
        }
    }

private:
    size_t capacity;
    std::list<KeyValuePair> cacheItemsList;
    std::unordered_map<Key, ListIterator> cacheItemsMap;
};

#endif // LRU_CACHE_H
