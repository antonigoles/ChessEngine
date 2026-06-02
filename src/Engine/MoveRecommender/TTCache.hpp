#pragma once
#include <cstdint>
#include <vector>

enum TTFlag : uint8_t {
    TT_EXACT = 0,
    TT_ALPHA = 1,
    TT_BETA = 2
};

#pragma pack(push, 1)
struct TTEntry {
    uint64_t key;
    int16_t score; 
    uint16_t move;
    uint8_t depth;
    uint8_t flag;
    uint16_t age;
};
#pragma pack(pop)


class TTCache
{
private:
    std::vector<TTEntry> table;
    uint64_t mask;
    uint16_t current_age;

public:
    TTCache(size_t size_in_mb) {
        current_age = 0;
        size_t num_entries = (size_in_mb * 1024 * 1024) / sizeof(TTEntry);
        size_t power_of_two = 1;
        while ((power_of_two * 2) <= num_entries) {
            power_of_two *= 2;
        }
        table.resize(power_of_two);
        mask = power_of_two - 1; 
        clear();
    }

    void clear() {
        for (auto& entry : table) {
            entry.key = 0;
            entry.depth = 0;
            entry.flag = TT_EXACT;
            entry.age = 0;
            entry.move = 0; 
        }
    }

    void increment_age() {
        current_age++;
    }

    inline TTEntry* probe(uint64_t key) {
        return &table[key & mask]; 
    }

    inline void store(uint64_t key, int depth, int score, uint16_t move, TTFlag flag) {
        TTEntry* entry = probe(key);

        if (entry->key != key || depth >= entry->depth || entry->age != current_age) {
            entry->key = key;
            entry->score = score;
            entry->depth = depth;
            entry->flag = flag;
            entry->age = current_age;
            
            if (move != 0) {
                entry->move = move;
            }
        }
    }
};