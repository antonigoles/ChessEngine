#pragma once
#include "Engine/Support/ChessMove.hpp"
#include "Engine/Support/Consts.hpp"
#include "Engine/Support/GameState/GameState.hpp"

struct ScoredMove {
    ChessMove move;
    int64_t score;

    bool operator<(const ScoredMove& other) const {
        return score > other.score; 
    }
};

namespace MoveOrdering {
    const int HASH_MOVE_BONUS = 1000000;
    const int PROMOTION_BONUS = 900000;
    const int CAPTURE_BONUS   = 100000;
    const int KILLER_1_BONUS  = 90000;
    const int KILLER_2_BONUS  = 80000;
    const int CASLTE_BONUS = 70000;
    const int CHECK_BONUS     = 50000;
    
    // MVV-LVA Table
    const int MVV_LVA_TABLE[6][6] = {
        // Atk: N,    B,    R,    Q,    P,    K
        { 205,  204,  203,  202,  206,  201 }, // Vic: KNIGHT (0)
        { 305,  304,  303,  302,  306,  301 }, // Vic: BISHOP (1)
        { 405,  404,  403,  402,  406,  401 }, // Vic: ROOK   (2)
        { 505,  504,  503,  502,  506,  501 }, // Vic: QUEEN  (3)
        { 105,  104,  103,  102,  106,  101 }, // Vic: PAWN   (4)
        {   0,    0,    0,    0,    0,    0 }  // Vic: KING   (5) - does not happen
    };

    inline int64_t score_move(
        const GameState& state, 
        const ChessMove& move, 
        int depth, 
        uint16_t hash_move,         
        const ChessMove killer_moves[64][2],            
        const int history_table[2][64][64],
        bool gives_check
    ) {
        if (move.data == hash_move) return HASH_MOVE_BONUS;

        int64_t score = 0;
        int64_t from = move.get_from();
        int64_t to = move.get_to();
        
        Color us = state.aux.get_turn();
        Color them = static_cast<Color>(!us);
        
        bool is_capture = (state.occupancy[them] & (1ULL << to)) != 0;
        bool is_en_passant = (!is_capture && move.get_promotion() == KNIGHT && !move.has_promotion() && to == state.aux.en_passant_square() && state.aux.can_en_passant());

        if (move.has_promotion()) {
            return PROMOTION_BONUS + (move.get_promotion() == QUEEN ? 100 : 0);
        }

        if (is_capture) {
            ChessPiece attacker = PAWN;
            ChessPiece victim = PAWN;

            for (int p = 0; p < 6; p++) {
                if (state.bitboards[us][p] & (1ULL << from)) {
                    attacker = static_cast<ChessPiece>(p);
                    break;
                }
            }

            for (int p = 0; p < 6; p++) {
                if (state.bitboards[them][p] & (1ULL << to)) {
                    victim = static_cast<ChessPiece>(p);
                    break;
                }
            }

            return CAPTURE_BONUS + MVV_LVA_TABLE[victim][attacker];
        } 
        else if (is_en_passant) {
            return CAPTURE_BONUS + MVV_LVA_TABLE[PAWN][PAWN]; 
        }

        // Silent Killers
        if (move.data == killer_moves[depth][0].data) return KILLER_1_BONUS;
        if (move.data == killer_moves[depth][1].data) return KILLER_2_BONUS;

        // is castle
        if ((state.bitboards[us][KING] & 1 << from) && (std::abs(from - to) == 2)) {
            return CASLTE_BONUS;
        };

        if (gives_check) {
            score += CHECK_BONUS;
        }
        

        return history_table[us][from][to];
    }

    inline int64_t score_move(
        const GameState& state, 
        const ChessMove& move,
        bool gives_check
    ) {
        int64_t score = 0;
        int from = move.get_from();
        int to = move.get_to();
        Color us = state.aux.get_turn();
        Color them = static_cast<Color>(!us);

        if (move.has_promotion()) {
            score += PROMOTION_BONUS + (move.get_promotion() == QUEEN ? 100 : 0);
        }

        bool is_capture = (state.occupancy[them] & (1ULL << to)) != 0;
        bool is_en_passant = (!is_capture && move.get_promotion() == KNIGHT && !move.has_promotion() && to == state.aux.en_passant_square() && state.aux.can_en_passant());

        if (is_capture || is_en_passant) {
            ChessPiece attacker = PAWN;
            ChessPiece victim = PAWN;

            for (int p = 0; p < 6; p++) {
                if (state.bitboards[us][p] & (1ULL << from)) {
                    attacker = static_cast<ChessPiece>(p);
                    break;
                }
            }

            if (is_capture) {
                for (int p = 0; p < 6; p++) {
                    if (state.bitboards[them][p] & (1ULL << to)) {
                        victim = static_cast<ChessPiece>(p);
                        break;
                    }
                }
            }

            score += CAPTURE_BONUS + MVV_LVA_TABLE[victim][attacker];
        }

        if (gives_check) {
            score += CHECK_BONUS;
        }

        return score;
    }
}