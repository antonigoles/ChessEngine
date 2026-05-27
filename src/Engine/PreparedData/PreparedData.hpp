#pragma once
#include "Engine/Support/GameState/GameState.hpp"
#include <cstdint>

struct Magic {
    uint64_t* attacks;
    uint64_t mask;
    uint64_t magic;
    uint8_t shift;
};

class Zobrist {
public:
    inline static uint64_t piece_keys[2][6][64];
    inline static uint64_t en_passant_keys[8];
    inline static uint64_t castle_keys[16];
    inline static uint64_t side_to_move_key;

    static void init(); 
    static uint64_t generate_key(const GameState& state);
};

class PreparedData
{
private:
    static void calculate_knights();

    static void calculate_rooks();

    static void calculate_bishops();

    static void calculate_kings();

    static void calculate_pawns();

    static void calculate_rooks_magic();

    static void calculate_bishops_magic();

    static void pawn_structure_masks();

public:
    inline static uint64_t pawns_attack_table[2][64];
    inline static uint64_t king_attack_table[64];
    inline static uint64_t knight_attack_table[64];
    inline static uint64_t rook_attack_table[64];
    inline static uint64_t bishop_attack_table[64];

    inline static Magic rook_magic_table[64];
    inline static Magic bishop_magic_table[64];

    inline static uint64_t king_safety_mask[2][64];

    inline static uint64_t isolated_pawn_mask[64];

    static void run_calculations();
};