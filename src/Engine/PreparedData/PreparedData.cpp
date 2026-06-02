#include "Engine/Support/Consts.hpp"
#include <Engine/PreparedData/PreparedData.hpp>
#include <cstdint>
#include <format>
#include <iostream>
#include <Engine/PreparedData/Zobrist.hpp>
#include <vector>

void PreparedData::calculate_knights()
{
    for (uint64_t i = 0; i<64; i++) {
        uint64_t move_mask = 0;
        uint64_t x = i % 8;
        uint64_t y = i / 8;
        std::vector<std::pair<uint64_t, uint64_t>> moves = {
            {-2, 1}, {2, 1}, {2, -1}, {-2, -1},
            {-1, 2}, {1, 2}, {1, -2}, {-1, -2}
        };
        for (auto move : moves) {
            int64_t xp = x + move.first;
            int64_t yp = y + move.second;

            if (xp < 0 || xp > 7 || yp < 0 || yp > 7) {
                continue;
            } 

            uint64_t m = xp + yp * 8;
            move_mask |= (1ULL << m);
        }

        PreparedData::knight_attack_table[i] = move_mask;
    }
}

void PreparedData::calculate_kings()
{
    for (uint64_t i = 0; i<64; i++) {
        uint64_t move_mask = 0;
        uint64_t x = i % 8;
        uint64_t y = i / 8;
        std::vector<std::pair<uint64_t, uint64_t>> moves = {
            {-1, 1}, {1, 1}, {-1, -1}, {1, -1},
            {0, 1}, {1, 0}, {-1, 0}, {0, -1},
        };
        for (auto move : moves) {
            int64_t xp = x + move.first;
            int64_t yp = y + move.second;

            if (xp < 0 || xp > 7 || yp < 0 || yp > 7) {
                continue;
            } 

            uint64_t m = xp + yp * 8;
            move_mask |= (1ULL << m);
        }

        PreparedData::king_attack_table[i] = move_mask;
    } 
};

void PreparedData::calculate_pawns()
{
    for (int64_t j = -1; j <= 1; j+=2) {
        for (uint64_t i = 0; i<64; i++) {
            uint64_t move_mask = 0;
            uint64_t x = i % 8;
            uint64_t y = i / 8;
            std::vector<std::pair<uint64_t, uint64_t>> moves = {
                {-1, 1}, {1, 1}
            };
            for (auto move : moves) {
                int64_t xp = x + move.first;
                int64_t yp = y + move.second * j;

                if (xp < 0 || xp > 7 || yp < 0 || yp > 7) {
                    continue;
                } 

                uint64_t m = xp + yp * 8;
                move_mask |= (1ULL << m);
            }

            PreparedData::pawns_attack_table[(j + 1) / 2][i] = move_mask;
        } 
    }
};

static uint64_t generate_true_rook_attacks(int sq, uint64_t occ) {
    uint64_t attacks = 0;
    int r = sq / 8, c = sq % 8;
    for (int i = r + 1; i < 8; ++i) { attacks |= (1ULL << (i * 8 + c)); if (occ & (1ULL << (i * 8 + c))) break; }
    for (int i = r - 1; i >= 0; --i) { attacks |= (1ULL << (i * 8 + c)); if (occ & (1ULL << (i * 8 + c))) break; }
    for (int i = c + 1; i < 8; ++i) { attacks |= (1ULL << (r * 8 + i)); if (occ & (1ULL << (r * 8 + i))) break; }
    for (int i = c - 1; i >= 0; --i) { attacks |= (1ULL << (r * 8 + i)); if (occ & (1ULL << (r * 8 + i))) break; }
    return attacks;
}

static uint64_t generate_true_bishop_attacks(int sq, uint64_t occ) {
    uint64_t attacks = 0;
    int r = sq / 8, c = sq % 8;
    for (int i = r + 1, j = c + 1; i < 8 && j < 8; ++i, ++j) { attacks |= (1ULL << (i * 8 + j)); if (occ & (1ULL << (i * 8 + j))) break; }
    for (int i = r + 1, j = c - 1; i < 8 && j >= 0; ++i, --j) { attacks |= (1ULL << (i * 8 + j)); if (occ & (1ULL << (i * 8 + j))) break; }
    for (int i = r - 1, j = c + 1; i >= 0 && j < 8; --i, ++j) { attacks |= (1ULL << (i * 8 + j)); if (occ & (1ULL << (i * 8 + j))) break; }
    for (int i = r - 1, j = c - 1; i >= 0 && j >= 0; --i, --j) { attacks |= (1ULL << (i * 8 + j)); if (occ & (1ULL << (i * 8 + j))) break; }
    return attacks;
}

static uint64_t get_bishop_mask(int sq) {
    uint64_t mask = 0ULL;
    int tr = sq / 8;
    int tf = sq % 8;

    for (int r = tr + 1, f = tf + 1; r <= 6 && f <= 6; r++, f++)  mask |= (1ULL << (r * 8 + f));
    for (int r = tr + 1, f = tf - 1; r <= 6 && f >= 1; r++, f--)  mask |= (1ULL << (r * 8 + f));
    for (int r = tr - 1, f = tf + 1; r >= 1 && f <= 6; r--, f++)  mask |= (1ULL << (r * 8 + f));
    for (int r = tr - 1, f = tf - 1; r >= 1 && f >= 1; r--, f--)  mask |= (1ULL << (r * 8 + f));

    return mask;
}

static uint64_t get_rook_mask(int sq) {
    uint64_t mask = 0ULL;
    int tr = sq / 8;
    int tf = sq % 8;

    for (int r = tr + 1; r <= 6; r++) mask |= (1ULL << (r * 8 + tf));
    for (int r = tr - 1; r >= 1; r--) mask |= (1ULL << (r * 8 + tf));
    for (int f = tf + 1; f <= 6; f++) mask |= (1ULL << (tr * 8 + f));
    for (int f = tf - 1; f >= 1; f--) mask |= (1ULL << (tr * 8 + f));

    return mask;
}

static uint64_t make_padding_mask(int sq)
{
    // TODO: Implement
    return 0;
}

void PreparedData::calculate_rooks()
{
    for (uint64_t i = 0; i<64; i++) {
        PreparedData::rook_attack_table[i] = get_rook_mask(i);
    }
}

void PreparedData::calculate_bishops()
{
   for (uint64_t i = 0; i<64; i++) {
        PreparedData::bishop_attack_table[i] = get_bishop_mask(i);
    } 
}

uint64_t random_uint64() {
    static uint64_t state = 1070372ULL;
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    return state * 2685821657736338717LL;
}

static uint64_t rook_attacks_storage[102400];
static uint64_t bishop_attacks_storage[5248];

void PreparedData::calculate_rooks_magic()
{
    int pool_offset = 0;

    for (int sq = 0; sq < 64; sq++) {
        uint64_t mask = PreparedData::rook_attack_table[sq]; 
        rook_magic_table[sq].mask = mask;
        
        int bits = std::popcount(mask);
        rook_magic_table[sq].shift = 64 - bits;
        rook_magic_table[sq].attacks = &rook_attacks_storage[pool_offset];
        
        int num_configs = 1 << bits;
        pool_offset += num_configs;

        std::vector<uint64_t> used_attacks(num_configs, 0ULL);
        
        std::vector<uint64_t> occupancies(num_configs);
        std::vector<uint64_t> real_attacks(num_configs);
        
        uint64_t b = 0;
        for (int i = 0; i < num_configs; i++) {
            occupancies[i] = b;
            real_attacks[i] = generate_true_rook_attacks(sq, b);
            b = (b - mask) & mask;
        }

        int attempts = 0;
        bool has_failed = true;

        while (attempts < 100000000) {
            attempts++;
            
            uint64_t magic_candidate = random_uint64() & random_uint64() & random_uint64();
            
            std::fill(used_attacks.begin(), used_attacks.end(), 0ULL);
            has_failed = false;

            for (int i = 0; i < num_configs; i++) {
                uint64_t index = (occupancies[i] * magic_candidate) >> rook_magic_table[sq].shift;
                
                if (used_attacks[index] == 0ULL) {
                    used_attacks[index] = real_attacks[i];
                } else if (used_attacks[index] != real_attacks[i]) {
                    has_failed = true;
                    break;
                }
            }

            if (!has_failed) {
                rook_magic_table[sq].magic = magic_candidate;
                for (int i = 0; i < num_configs; i++) {
                    uint64_t index = (occupancies[i] * magic_candidate) >> rook_magic_table[sq].shift;
                    rook_magic_table[sq].attacks[index] = real_attacks[i];
                }
                break;
            }
        }

        if (has_failed) {
            std::cerr << "Failed to find magic number for square " << sq << "!\n";
            exit(-1);
        }
    }
}

void PreparedData::calculate_bishops_magic()
{
    int pool_offset = 0;

    for (int sq = 0; sq < 64; sq++) {
        uint64_t mask = PreparedData::bishop_attack_table[sq]; 
        bishop_magic_table[sq].mask = mask;
        
        int bits = std::popcount(mask);
        bishop_magic_table[sq].shift = 64 - bits;
        bishop_magic_table[sq].attacks = &bishop_attacks_storage[pool_offset];
        
        int num_configs = 1 << bits;
        pool_offset += num_configs;

        std::vector<uint64_t> used_attacks(num_configs, 0ULL);
        
        std::vector<uint64_t> occupancies(num_configs);
        std::vector<uint64_t> real_attacks(num_configs);
        
        uint64_t b = 0;
        for (int i = 0; i < num_configs; i++) {
            occupancies[i] = b;
            real_attacks[i] = generate_true_bishop_attacks(sq, b);
            b = (b - mask) & mask;
        }

        int attempts = 0;
        bool has_failed = true;

        while (attempts < 100000000) {
            attempts++;
            
            uint64_t magic_candidate = random_uint64() & random_uint64() & random_uint64();
            
            std::fill(used_attacks.begin(), used_attacks.end(), 0ULL);
            has_failed = false;

            for (int i = 0; i < num_configs; i++) {
                uint64_t index = (occupancies[i] * magic_candidate) >> bishop_magic_table[sq].shift;
                
                if (used_attacks[index] == 0ULL) {
                    used_attacks[index] = real_attacks[i];
                } else if (used_attacks[index] != real_attacks[i]) {
                    has_failed = true;
                    break;
                }
            }

            if (!has_failed) {
                bishop_magic_table[sq].magic = magic_candidate;
                for (int i = 0; i < num_configs; i++) {
                    uint64_t index = (occupancies[i] * magic_candidate) >> bishop_magic_table[sq].shift;
                    bishop_magic_table[sq].attacks[index] = real_attacks[i];
                }
                break;
            }
        }

        if (has_failed) {
            std::cerr << "Failed to find magic number for square " << sq << "!\n";
            exit(-1);
        }
    }
}

void PreparedData::pawn_structure_masks()
{
    // 1. King safety masks (shields)
    constexpr uint64_t FILE_A = 0x0101010101010101ULL;
    constexpr uint64_t FILE_H = 0x8080808080808080ULL;

    for (int pos = 0; pos < 64; pos++) {
        uint64_t king_bit = 1ULL << pos;

        uint64_t white_shield = 
            (king_bit << 8)
            | ((king_bit & ~FILE_A) << 7)
            | ((king_bit & ~FILE_H) << 9);

        PreparedData::king_safety_mask[Color::WHITE][pos] = white_shield;

        uint64_t black_shield = 
            (king_bit >> 8)
            | ((king_bit & ~FILE_A) >> 9)
            | ((king_bit & ~FILE_H) >> 7);

        PreparedData::king_safety_mask[Color::BLACK][pos] = black_shield;
    }

    // 2. Isolated pawn mask
    for (int pos = 0; pos < 64; pos++) {
        int file = pos % 8;
        uint64_t center_file = FILE_A << file;
        uint64_t west_file = (center_file & ~FILE_A) >> 1;
        uint64_t east_file = (center_file & ~FILE_H) << 1;
        PreparedData::isolated_pawn_mask[pos] = west_file | east_file;
    }
}

void PreparedData::dump_magic()
{
    // 1. dump storage
    // static uint64_t rook_attacks_storage[102400];
    // static uint64_t bishop_attacks_storage[5248];
    std::cout << "#pragma once\n";

    std::cout << "static uint64_t rook_attacks_storage[102400] = {\n";
    for (int i = 0; i < 102400; i++) {
        if ((i+1) % 10 == 0) std::cout << "\n";
        std::cout << std::format("{:#x}", rook_attacks_storage[i]) << (i+1 == 102400 ? "" : ",");
    }
    std::cout << "};\n";

    std::cout << "static uint64_t bishop_attacks_storage[5248] = {\n";
    for (int i = 0; i < 5248; i++) {
        if ((i+1) % 10 == 0) std::cout << "\n";
        std::cout << std::format("{:#x}", bishop_attacks_storage[i]) << (i+1 == 102400 ? "" : ",");
    }
    std::cout << "};\n";

    // 2. dump magic (rooks first)
    std::cout << "Magic rook_magic_table[64] = {\n";
    for (int i = 0; i<64; i++) {
        int att_idx = PreparedData::rook_magic_table[i].attacks - rook_attacks_storage;
        std::cout << "    { &rook_attacks_storage[" << att_idx << "], " 
            << std::format("{:#x}", PreparedData::rook_magic_table[i].mask) << ", "
            << std::format("{:#x}", PreparedData::rook_magic_table[i].magic) << ", "
            << (int)PreparedData::rook_magic_table[i].shift
            << " },\n";
    }
    std::cout << "};\n";

    // 3. dump magic (bishops now)
    std::cout << "Magic bishop_magic_table[64] = {\n";
    for (int i = 0; i<64; i++) {
        int att_idx = PreparedData::bishop_magic_table[i].attacks - bishop_attacks_storage;
        std::cout << "    { &bishop_attacks_storage[" << att_idx << "], " 
            << std::format("{:#x}", PreparedData::bishop_magic_table[i].mask) << ", "
            << std::format("{:#x}", PreparedData::bishop_magic_table[i].magic) << ", "
            << (int)PreparedData::bishop_magic_table[i].shift
            << " },\n";
    }
    std::cout << "};\n";
}

static inline void calculate_king_safety_ring()
{
    for (int sq = 0; sq < 64; sq++) {
        uint64_t inner_ring = 0;
        uint64_t outer_ring = 0;
        int k_file = sq % 8;
        int k_rank = sq / 8;
        int start_rank = std::max(0, k_rank - 2);
        int end_rank   = std::min(7, k_rank + 2);
        int start_file = std::max(0, k_file - 2);
        int end_file   = std::min(7, k_file + 2);
        for (int r = start_rank; r <= end_rank; r++) {
            for (int f = start_file; f <= end_file; f++) {
                int target_sq = r * 8 + f;
                uint64_t target_mask = 1ULL << target_sq;
                if (std::abs(r - k_rank) <= 1 && std::abs(f - k_file) <= 1) {
                    inner_ring |= target_mask;
                }
                outer_ring |= target_mask;
            }
        }

        PreparedData::king_safety_ring_mask_small[sq] = inner_ring;
        PreparedData::king_safety_ring_mask_big[sq] = inner_ring | outer_ring;
    }
}

static void calc_pst_tables() 
{
    for (int p = 0; p < 6; p++) {
        for (int sq = 0; sq < 64; sq++) {
            PreparedData::mg_pst_table[WHITE][p][sq] = pst_mg_value[p] + PST_TABLE[0][p][sq];
            PreparedData::eg_pst_table[WHITE][p][sq] = pst_eg_value[p] + PST_TABLE[1][p][sq];
            
            PreparedData::mg_pst_table[BLACK][p][sq] = pst_mg_value[p] + PST_TABLE[0][p][sq ^ 56];
            PreparedData::eg_pst_table[BLACK][p][sq] = pst_eg_value[p] + PST_TABLE[1][p][sq ^ 56];
        }
    }
}

static void passed_pawn_calculations()
{
    for (int sq = 0; sq < 64; sq++) {
            int file = sq % 8;
            int rank = sq / 8;

            uint64_t white_mask = 0ULL;
            uint64_t black_mask = 0ULL;

            for (int r = rank + 1; r < 8; r++) {
                white_mask |= (1ULL << (r * 8 + file));
                if (file > 0) white_mask |= (1ULL << (r * 8 + file - 1));
                if (file < 7) white_mask |= (1ULL << (r * 8 + file + 1)); 
            }

            for (int r = rank - 1; r >= 0; r--) {
                black_mask |= (1ULL << (r * 8 + file));
                if (file > 0) black_mask |= (1ULL << (r * 8 + file - 1));
                if (file < 7) black_mask |= (1ULL << (r * 8 + file + 1));
            }

            PreparedData::passed_pawn_masks[WHITE][sq] = white_mask;
            PreparedData::passed_pawn_masks[BLACK][sq] = black_mask;
        }
}

void PreparedData::run_calculations()
{
    PreparedData::calculate_knights();
    std::cerr << "calculate_knights\n";

    PreparedData::calculate_rooks();
    std::cerr << "calculate_rooks\n";

    PreparedData::calculate_bishops();
    std::cerr << "calculate_bishops\n";
    // PreparedData::calculate_rooks_magic();
    // PreparedData::calculate_bishops_magic();
    PreparedData::calculate_kings();
    calculate_king_safety_ring();
    std::cerr << "calculate_kings\n";
    
    PreparedData::calculate_pawns();
    std::cerr << "calculate_pawns\n";

    passed_pawn_calculations();
    std::cerr << "passed_pawn_calculations\n";
    
    PreparedData::pawn_structure_masks();
    std::cerr << "pawn_structure_masks\n";
    
    Zobrist::init();
    std::cerr << "Zobrist\n";

    calc_pst_tables();
    std::cerr << "calc_pst_tables\n";
};