#pragma once
#include "Engine/Support/Consts.hpp"
#include <cstdint>
#include <iostream>

class BitsUtil
{
public:
    static void write_bitboard(uint64_t bitboard)
    {
        for (int64_t y = 7; y >= 0; y--) {
            for (int64_t x = 0; x < 8; x++) {
                uint64_t mask = 1ULL << (y * 8 + x);
                std::cout << (bitboard & mask ? '1' : '0');
            }
            std::cout << "\n";
        }
    }

    inline static constexpr uint64_t FILE_A = 0x0101010101010101ULL;
    inline static constexpr uint64_t FILE_H = 0x8080808080808080ULL;

    template <int Us>
    inline static constexpr uint64_t shift_push(uint64_t b) {
        return Us == WHITE ? (b << 8) : (b >> 8);
    }

    template <int Us>
    inline static constexpr uint64_t shift_cap_west(uint64_t b) {
        return Us == WHITE ? ((b & ~FILE_A) << 7) : ((b & ~FILE_A) >> 9);
    }

    template <int Us>
    inline static constexpr uint64_t shift_cap_east(uint64_t b) {
        return Us == WHITE ? ((b & ~FILE_H) << 9) : ((b & ~FILE_H) >> 7);
    }
};