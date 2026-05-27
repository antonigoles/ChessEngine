#pragma once
#include "Utils/BitsUtil.hpp"
#include <cstdint>
#include <vector>
#include <bit>
#include <Engine/Support/GameState/GameState.hpp>
#include <Engine/Support/ChessMove.hpp>
#include <Engine/PreparedData/PreparedData.hpp>

template <int Us>
void generate_pawn_moves(const GameState& state, std::vector<ChessMove>& moves) 
{
    constexpr int Them = Us ^ 1;
    constexpr uint64_t RANK_3_OR_6 = (Us == 1) ? 0x0000000000FF0000ULL : 0x0000FF0000000000ULL;
    constexpr uint64_t RANK_7_OR_2 = (Us == 1) ? 0x00FF000000000000ULL : 0x000000000000FF00ULL;

    uint64_t pawns = state.bitboards[Us][ChessPiece::PAWN];
    uint64_t enemies = state.occupancy[Them];
    uint64_t empty = ~state.total_occupancy;

    uint64_t non_promo_pawns = pawns & ~RANK_7_OR_2;
    
    uint64_t single_push = BitsUtil::shift_push<Us>(non_promo_pawns) & empty;
    uint64_t double_push = BitsUtil::shift_push<Us>(single_push & RANK_3_OR_6) & empty;
    
    uint64_t cap_west = BitsUtil::shift_cap_west<Us>(non_promo_pawns) & enemies;
    uint64_t cap_east = BitsUtil::shift_cap_east<Us>(non_promo_pawns) & enemies;

    constexpr int offset_push = (Us == 1) ? -8 : 8;
    constexpr int offset_dp   = (Us == 1) ? -16 : 16;
    constexpr int offset_west = (Us == 1) ? -7 : 9;
    constexpr int offset_east = (Us == 1) ? -9 : 7;

    uint64_t bb = single_push;
    while (bb) {
        int to = std::countr_zero(bb);
        moves.emplace_back(to + offset_push, to); 
        bb &= bb - 1;
    }

    bb = double_push;
    while (bb) {
        int to = std::countr_zero(bb);
        moves.emplace_back(to + offset_dp, to);
        bb &= bb - 1;
    }

    bb = cap_west;
    while (bb) {
        int to = std::countr_zero(bb);
        moves.emplace_back(to + offset_west, to);
        bb &= bb - 1;
    }

    bb = cap_east;
    while (bb) {
        int to = std::countr_zero(bb);
        moves.emplace_back(to + offset_east, to);
        bb &= bb - 1;
    }

    uint64_t promo_pawns = pawns & RANK_7_OR_2;
    if (promo_pawns) {
        uint64_t promo_push = BitsUtil::shift_push<Us>(promo_pawns) & empty;
        uint64_t promo_cap_west = BitsUtil::shift_cap_west<Us>(promo_pawns) & enemies;
        uint64_t promo_cap_east = BitsUtil::shift_cap_east<Us>(promo_pawns) & enemies;

        auto add_promotions = [&](uint64_t p_bb, int offset) {
            while (p_bb) {
                int to = std::countr_zero(p_bb);
                int from = to + offset;
                
                moves.emplace_back(from, to, ChessPiece::QUEEN, true);
                moves.emplace_back(from, to, ChessPiece::ROOK, true);
                moves.emplace_back(from, to, ChessPiece::BISHOP, true);
                moves.emplace_back(from, to, ChessPiece::KNIGHT, true);
                
                p_bb &= p_bb - 1;
            }
        };

        add_promotions(promo_push, offset_push);
        add_promotions(promo_cap_west, offset_west);
        add_promotions(promo_cap_east, offset_east);
    }

    if (state.aux.can_en_passant()) {
        uint8_t ep_sq = state.aux.en_passant_square();
        
        uint64_t attackers = PreparedData::pawns_attack_table[Them][ep_sq] & pawns;
        
        while (attackers) {
            int from = std::countr_zero(attackers);
            moves.emplace_back(from, ep_sq, true); 
            attackers &= attackers - 1;
        }
    }
}