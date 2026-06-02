#pragma once
#include <Engine/Support/GameState/GameState.hpp>
#include <cstdint>

class Zobrist {
public:
    inline static uint64_t piece_keys[2][6][64];
    inline static uint64_t en_passant_keys[8];
    inline static uint64_t castle_keys[16];
    inline static uint64_t side_to_move_key;

    static void init(); 
    static uint64_t generate_key(GameState& state);

    static bool is_polyglot_ep_active(GameState& state);
};