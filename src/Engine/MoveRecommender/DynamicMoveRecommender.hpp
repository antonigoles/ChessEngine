#pragma once

#include "Engine/MoveGenerator/MoveGenerator.hpp"
#include "Engine/MoveRecommender/MoveOrdering.hpp"
#include "Engine/StateTransformer/StateTransformer.hpp"
#include "Engine/Support/ChessMove.hpp"
#include "Engine/Support/GameState/GameState.hpp"
#include <bit>
#include <chrono>
#include <optional>
#include <sys/types.h>

class DynamicMoveRecommender
{
public:
    const float INF_MIN = -1000000.0f;
    const float INF_MAX = +1000000.0f;

private:
    std::chrono::steady_clock::time_point search_start;
    float time_limit;

    const ssize_t MAX_CONSECUTIVES_CHECKS_LIMIT = 24;
    const ssize_t OVERAL_MAX_DEPTH = 100;

    float alpha;
    float *piece_values;

public:
    DynamicMoveRecommender(float alpha, float *piece_values) : time_limit(0.0), alpha(alpha), piece_values(piece_values) {
        reset_round_timer();
    }

    void reset_round_timer()
    {
        search_start = std::chrono::steady_clock::now();
    }

    bool out_of_time()
    {
        auto now = std::chrono::steady_clock::now();
        auto time_passed = std::chrono::duration<double>(now - this->search_start).count();
        return time_passed >= this->time_limit * 0.93;
    }

    float eval_state(const GameState& state) 
    {
        // Count material
        float eval = 0.0f;

        for (int p = 0; p < 6; p++) {
            eval += std::popcount(state.bitboards[WHITE][p]) * piece_values[p];
            eval -= std::popcount(state.bitboards[BLACK][p]) * piece_values[p];
        }

        ssize_t a = MoveGenerator::generate_pseudo_legal_moves(state).size();
        GameState clone = state;
        clone.aux.set_turn(clone.aux.get_turn() == WHITE ? BLACK : WHITE);
        ssize_t b = MoveGenerator::generate_pseudo_legal_moves(clone).size();

        eval += (float)(a-b) * alpha;

        return eval;
    }

    float quiescence_search(const GameState& state, float alpha, float beta, ssize_t consecutive_checks = 0, ssize_t depth = 0) {
        float current_eval = eval_state(state);
        if (out_of_time()) return current_eval;
        if (consecutive_checks >= MAX_CONSECUTIVES_CHECKS_LIMIT) return current_eval;
        if (depth >= OVERAL_MAX_DEPTH) return current_eval;
        
        Color us = state.aux.get_turn();
        Color them = static_cast<Color>(!us);

        if (us == WHITE) {
            if (current_eval >= beta) return beta;
            if (current_eval > alpha) alpha = current_eval;
        } else {
            if (current_eval <= alpha) return alpha;
            if (current_eval < beta) beta = current_eval;
        }

        std::vector<ChessMove> moves = MoveGenerator::generate_pseudo_legal_moves(state);
        std::vector<ScoredMove> captures;
        captures.reserve(10);

        for (const auto& move : moves) {
            int to = move.get_to();
            bool is_capture = (state.occupancy[them] & (1ULL << to)) != 0;
            bool is_en_passant = (!is_capture && move.get_promotion() == KNIGHT && !move.has_promotion() && to == state.aux.en_passant_square() && state.aux.can_en_passant());
            bool gives_check = StateTransformer::is_king_attacked_after_move(state, move);
            bool moves_away_from_check = state.is_checked && !StateTransformer::is_king_attacked_after_move(state, move);

            if (is_capture || is_en_passant || move.has_promotion() || gives_check || moves_away_from_check) {
                captures.push_back({move, MoveOrdering::score_move(state, move, gives_check)});
            }
        }

        std::sort(captures.begin(), captures.end());


        float best_score = current_eval; 

        ssize_t legal_moves_count = 0;

        for (const auto& sm : captures) {
            if (out_of_time()) return current_eval;
            GameState next_state = state;
            if (!StateTransformer::apply_move(next_state, sm.move)) {
                continue;
            }

            legal_moves_count++;

            consecutive_checks = (next_state.is_checked || state.is_checked) ? consecutive_checks + 1 : 0;
            float score = quiescence_search(next_state, alpha, beta, consecutive_checks, depth + 1);

            if (us == WHITE) {
                if (score > best_score) best_score = score;
                if (score >= beta) return beta;
                if (score > alpha) alpha = score;
            } else {
                if (score < best_score) best_score = score;
                if (score <= alpha) return alpha;
                if (score < beta) beta = score;
            }
        }

        if (legal_moves_count == 0) {
            if (state.is_checked) {
                // Checkmate
                return state.aux.get_turn() == WHITE ? INF_MIN : INF_MAX;
            } else {
                // the situation has calmed down
                return current_eval;
            }
        }

        return best_score;
    }

    std::optional<ChessMove> recommend_next_move(const GameState& state, float time_per_move) 
    {
        this->reset_round_timer();
        this->time_limit = time_per_move;
        auto moves = MoveGenerator::generate_pseudo_legal_moves(state);
        if (moves.empty()) return std::nullopt;
        std::optional<ChessMove> best_move = std::nullopt;
        float best_eval = state.aux.get_turn() == BLACK ? INF_MAX : INF_MIN;

        while (!moves.empty() && !out_of_time()) {
            GameState state_copy = state;
            ChessMove move = moves.back();
            moves.pop_back();
            if (!StateTransformer::apply_move(state_copy, move)) {
                continue;
            }
            float eval = this->quiescence_search(state_copy, INF_MIN, INF_MAX);
            
            if (state.aux.get_turn() == WHITE) {
                if (eval > best_eval) {
                    best_eval = eval;
                    best_move = move;
                }
            } else {
                if (eval < best_eval) {
                    best_eval = eval;
                    best_move = move;
                }
            }
        }

        return best_move;
    }

};