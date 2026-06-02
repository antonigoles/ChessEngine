#pragma once
#include <cstdint>
#include <string>

struct Magic {
    uint64_t* attacks;
    uint64_t mask;
    uint64_t magic;
    uint8_t shift;
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

    static void load_rooks_magic_from_file(const std::string& path);

    static void load_bishops_magic_from_file(const std::string& path);

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
    inline static uint64_t king_safety_mask_further[2][64];

    inline static uint64_t king_safety_ring_mask_small[64];
    inline static uint64_t king_safety_ring_mask_big[64];

    inline static uint64_t isolated_pawn_mask[64];

    inline static int64_t mg_pst_table[2][6][64];
    inline static int64_t eg_pst_table[2][6][64];

    inline static uint64_t passed_pawn_masks[2][64];

    static void run_calculations();

    static void dump_magic();
};