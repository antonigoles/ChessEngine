#include "Engine/Support/Consts.hpp"
#include <Engine/PreparedData/PreparedData.hpp>
#include <cstdint>
#include <iostream>
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

void PreparedData::run_calculations()
{
    PreparedData::calculate_knights();
    PreparedData::calculate_rooks();
    PreparedData::calculate_bishops();
    PreparedData::calculate_rooks_magic();
    PreparedData::calculate_bishops_magic();
    PreparedData::calculate_kings();
    PreparedData::calculate_pawns();
    PreparedData::pawn_structure_masks();
    Zobrist::init();
};

void Zobrist::init() 
{
    const int polyglot_piece_map[6] = {2, 4, 6, 8, 0, 10};

    for (int c = 0; c < 2; c++) {
        for (int p = 0; p < 6; p++) {
            int poly_piece = polyglot_piece_map[p] + (c == WHITE ? 1 : 0);
            for (int sq = 0; sq < 64; sq++) {
                piece_keys[c][p][sq] = Polyglot_Random64[64 * poly_piece + sq];
            }
        }
    }

    for (int file = 0; file < 8; file++) {
        en_passant_keys[file] = Polyglot_Random64[772 + file];
    }

    for (int i = 0; i < 16; i++) {
        uint64_t k = 0;
        if (i & 1) k ^= Polyglot_Random64[768];
        if (i & 2) k ^= Polyglot_Random64[769];
        if (i & 4) k ^= Polyglot_Random64[770];
        if (i & 8) k ^= Polyglot_Random64[771];
        castle_keys[i] = k;
    }

    side_to_move_key = Polyglot_Random64[780];
}; 

uint64_t Zobrist::generate_key(const GameState& state)
{
    uint64_t key = 0;

    for (int c = 0; c < 2; c++) {
        for (int p = 0; p < 6; p++) {
            uint64_t bb = state.bitboards[c][p];
            while (bb) {
                int sq = std::countr_zero(bb);
                key ^= piece_keys[c][p][sq];
                bb &= bb - 1; 
            }
        }
    }

    if (state.aux.can_en_passant()) {
        int ep_sq = state.aux.en_passant_square();
        int file = ep_sq % 8; 
        key ^= en_passant_keys[file];
    }

    int castle_rights = 0;
    if (state.aux.can_white_short_castle()) castle_rights |= 1;
    if (state.aux.can_white_long_castle())  castle_rights |= 2;
    if (state.aux.can_black_short_castle()) castle_rights |= 4;
    if (state.aux.can_black_long_castle())  castle_rights |= 8;
    
    key ^= castle_keys[castle_rights];

    if (state.aux.get_turn() == WHITE) { 
        key ^= side_to_move_key;
    }

    return key;
};
