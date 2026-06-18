#pragma once
#include <Engine/Support/ChessMove.hpp>
#include <Engine/Support/GameState/GameState.hpp>
#include <vector>
#include <cstdint>
#include "Engine/PreparedData/PreparedData.hpp"
#include <Engine/Support/Consts.hpp>

class MoveGenerator
{
private:
    template <Color Us>
    static std::vector<ChessMove> aux_generate_pseudo_legal_moves(const GameState& state);


public:
    static uint64_t get_rook_attacks(uint64_t from, uint64_t occupancy);

    static uint64_t get_bishop_attacks(uint64_t from, uint64_t occupancy);

    template <Color Us>
    static bool is_our_king_in_check(const GameState& state)
    {
        uint64_t king_sq = std::countr_zero(state.bitboards[Us][KING]);
        if constexpr (Us == WHITE) {
            return is_square_attacked<BLACK>(king_sq, state);
        } else {
            return is_square_attacked<WHITE>(king_sq, state);
        }
    }

    template <Color Them>
    static bool is_square_attacked(int sq, const GameState& state) 
    {
        constexpr Color Us = (Color)!Them; 
        if (PreparedData::pawns_attack_table[Us][sq] & state.bitboards[Them][PAWN]) return true;
        if (PreparedData::knight_attack_table[sq] & state.bitboards[Them][KNIGHT]) return true;
        if (PreparedData::king_attack_table[sq] & state.bitboards[Them][KING]) return true;
        uint64_t bishop_rays = get_bishop_attacks(sq, state.total_occupancy);
        if (bishop_rays & (state.bitboards[Them][BISHOP] | state.bitboards[Them][QUEEN])) return true;
        uint64_t rook_rays = get_rook_attacks(sq, state.total_occupancy);
        if (rook_rays & (state.bitboards[Them][ROOK] | state.bitboards[Them][QUEEN])) return true;
        return false;
    }

    static std::vector<ChessMove> generate_pseudo_legal_moves(const GameState& state);
};