#include "Engine/MoveGenerator/MoveGenerator.hpp"
#include "Engine/MoveRecommender/MoveOrdering.hpp"
#include "Engine/MoveRecommender/TTCache.hpp"
#include "Engine/PreparedData/Zobrist.hpp"
#include "Engine/StateTransformer/StateTransformer.hpp"
#include "Engine/Support/ChessMove.hpp"
#include "Engine/Support/Consts.hpp"
#include <Engine/MoveRecommender/MoveRecommender.hpp>
#include <cmath>
#include <cstdint>
#include <optional>
#include <sys/types.h>

MoveRecommender::MoveRecommender() : time_limit(0.0), tt_cache(64) {
    reset_round_timer();
};

void MoveRecommender::reset_round_timer()
{
    search_start = std::chrono::steady_clock::now();
}

bool MoveRecommender::out_of_time()
{
    if ((this->visited_nodes & 2047) != 0) return false;
    auto now = std::chrono::steady_clock::now();
    auto time_passed = std::chrono::duration<double>(now - this->search_start).count();
    return time_passed >= this->time_limit * 0.93;
}

inline static bool has_non_pawn_material(const GameState& state, Color us) {
    return (state.bitboards[us][KNIGHT] | 
            state.bitboards[us][BISHOP] | 
            state.bitboards[us][ROOK]   | 
            state.bitboards[us][QUEEN]) != 0;
}

float MoveRecommender::quiescence_search(const GameState& state, float alpha, float beta) {
    float stand_pat = state.eval_position();
    if (out_of_time()) return state.eval_position();
    
    Color us = state.aux.get_turn();
    Color them = static_cast<Color>(!us);

    if (us == WHITE) {
        if (stand_pat >= beta) return beta;
        if (stand_pat > alpha) alpha = stand_pat;
    } else {
        if (stand_pat <= alpha) return alpha;
        if (stand_pat < beta) beta = stand_pat;
    }

    std::vector<ChessMove> moves = MoveGenerator::generate_pseudo_legal_moves(state);
    std::vector<ScoredMove> captures;
    captures.reserve(10);

    for (const auto& move : moves) {
        int to = move.get_to();
        bool is_capture = (state.occupancy[them] & (1ULL << to)) != 0;
        bool is_en_passant = (!is_capture && move.get_promotion() == KNIGHT && !move.has_promotion() && to == state.aux.en_passant_square() && state.aux.can_en_passant());
        bool gives_check = StateTransformer::is_king_attacked_after_move(state, move);

        if (is_capture || is_en_passant || move.has_promotion() || gives_check) {
            captures.push_back({move, MoveOrdering::score_move(state, move, gives_check)});
        }
    }

    std::sort(captures.begin(), captures.end());


    float best_score = stand_pat; 

    for (const auto& sm : captures) {
        if (out_of_time()) return state.eval_position();
        GameState next_state = state;
        if (!StateTransformer::apply_move(next_state, sm.move)) {
            continue;
        }

        float score = quiescence_search(next_state, alpha, beta);

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

    return best_score;
}


float MoveRecommender::alpha_beta_search(
    const GameState &state, 
    std::vector<uint64_t>& history,
    ssize_t depth,
    float alpha,
    float beta,
    bool allow_null
) {
    this->visited_nodes++;
    auto moves = MoveGenerator::generate_pseudo_legal_moves(state);

    // The game is lost
    if (moves.empty() && state.is_checked) {
        return state.aux.get_turn() == WHITE ? -10000000.0f : 10000000.0f;
    }

    // Draw when I'm so ahead is bad
    if ((moves.empty() && !state.is_checked) || state.aux.half_move_count() >= 100) {
        return -0.01f;
    }

    if (this->out_of_time()) {
        return state.eval_position();
    }

    if (depth <= 0) {
        return quiescence_search(state, alpha, beta);
    }

    int limit = state.aux.half_move_count();
    int history_size = history.size();
    int checks_to_make = std::min(history_size, limit);

    for (int i = history_size - 2; i >= history_size - checks_to_make && i >= 0; i -= 2) {
        if (history[i] == state.zobrist_key) {
            return -0.01f;
        }
    }

    float original_alpha = alpha;

    TTEntry* tt_entry = tt_cache.probe(state.zobrist_key);
    bool tt_hit = (tt_entry->key == state.zobrist_key);
    uint16_t tt_move = 0;

    if (tt_hit) {
        tt_move = tt_entry->move;

        if (tt_entry->depth >= depth) {

            if (tt_entry->flag == TT_EXACT) {
                return tt_entry->score;
            }

            if (tt_entry->flag == TT_ALPHA && tt_entry->score <= alpha) {
                return tt_entry->score;
            }

            if (tt_entry->flag == TT_BETA && tt_entry->score >= beta) {
                return tt_entry->score;
            }
        }
    }

    // NULL MOVE PRUNING
    const int R = 2; // depth reduction
    if (allow_null && depth >= 3 && !state.is_checked && has_non_pawn_material(state, state.aux.get_turn())) {
        GameState null_state = state;

        // Switch sides
        Color them = static_cast<Color>(!state.aux.get_turn());
        null_state.aux.set_turn(them);
        null_state.zobrist_key ^= Zobrist::side_to_move_key;

        // ZERO OUT EN PASSANT
        if (null_state.aux.can_en_passant()) {
            int ep_sq = null_state.aux.en_passant_square();
            null_state.zobrist_key ^= Zobrist::en_passant_keys[ep_sq % 8];
            null_state.aux.clear_en_passant();
        }

        float null_score = alpha_beta_search(null_state, history, depth - 1 - R, alpha, beta, false);

        if (state.aux.get_turn() == WHITE) {
            if (null_score >= beta) return beta;
        } else {
            if (null_score <= alpha) return alpha;
        }
    }

    float best_eval = state.aux.get_turn() == BLACK ? std::numeric_limits<float>::infinity() : -std::numeric_limits<float>::infinity();

    std::vector<ScoredMove> scored_moves;
    for (auto& move : moves) {
        bool gives_check = StateTransformer::is_king_attacked_after_move(state, move);
        scored_moves.push_back({
            move, 
            MoveOrdering::score_move(state, move, depth, tt_move, this->killer_moves, this->history_table, gives_check)
        });
    }
    std::sort(scored_moves.begin(), scored_moves.end());

    std::optional<ChessMove> best_move;

    Color us = state.aux.get_turn();
    Color them = static_cast<Color>(!us);

    for (ssize_t i = 0; i<scored_moves.size(); i++) {
        if (this->out_of_time()) return state.eval_position();

        const auto& scored_move = scored_moves[i];

        GameState next_state = state;
        if (!StateTransformer::apply_move(next_state, scored_move.move)) {
            continue;
        };

        // LMR logic
        int to = scored_move.move.get_to();
        bool is_capture = (state.occupancy[them] & (1ULL << to)) != 0;
        bool is_promotion = scored_move.move.has_promotion();
        bool gives_check = StateTransformer::is_king_attacked_after_move(state, scored_move.move);
        bool is_quiet = !is_capture && !is_promotion && !gives_check;
        uint64_t check_extension = next_state.is_checked ? 1 : 0;
        float eval;

        history.push_back(state.zobrist_key);
        if (depth >= 3 && i >= 3 && is_quiet && !state.is_checked) {
            int64_t R = 0.75 + std::log(depth) * std::log(i) / 2.25;
            eval = alpha_beta_search(next_state, history, depth - 1 - R, alpha, beta, true);

            // re-search in case this move turned out to be interesting
            if ((eval > alpha && us == WHITE) || (eval < beta && us == BLACK)) {
                eval = alpha_beta_search(next_state, history, depth - 1, alpha, beta, true);
            }

        } else {
            eval = alpha_beta_search(next_state, history, depth - 1 + check_extension, alpha, beta, true);
        }
        history.pop_back();
        

        if (us == BLACK) {
            if (eval < best_eval) {
                best_eval = eval;
                best_move = scored_move.move;
            }
            beta = std::min(beta, eval);
        } else {
            if (eval > best_eval) {
                best_eval = eval;
                best_move = scored_move.move;
            }
            alpha = std::max(alpha, eval);
        }

        if (beta <= alpha) {
            ChessMove move = scored_move.move;
            bool is_capture = (state.occupancy[them] & (1ULL << move.get_to())) != 0;
            
            if (!is_capture && !move.has_promotion()) {
                
                int bonus = depth * depth;
                this->history_table[us][move.get_from()][move.get_to()] += bonus;
                
                if (this->history_table[us][move.get_from()][move.get_to()] > 1000000) {
                    for (int c = 0; c < 2; c++)
                        for (int f = 0; f < 64; f++)
                            for (int t = 0; t < 64; t++)
                                this->history_table[c][f][t] /= 2;
                }

                if (this->killer_moves[depth][0].data != move.data) {
                    this->killer_moves[depth][1] = this->killer_moves[depth][0];
                    this->killer_moves[depth][0] = move;
                }
            }
            
            break; 
        }
    }

    TTFlag flag;
    if (best_eval <= original_alpha) {
        flag = TT_ALPHA;
    } else if (best_eval >= beta) {
        flag = TT_BETA;
    } else {
        flag = TT_EXACT;
    }

    uint16_t best_move_data = best_move.has_value() ? best_move.value().data : 0;

    tt_cache.store(state.zobrist_key, depth, best_eval, best_move_data, flag);

    return best_eval;
}

std::optional<ChessMove> MoveRecommender::recommend_next_move(
    GameState &game_state, 
    double time_for_search, 
    std::vector<uint64_t>& history,
    int min_depth, int 
    max_depth
) {
    this->visited_nodes = 0;
    this->time_limit = time_for_search;

    this->clear_heuristics();
    tt_cache.increment_age();

    auto moves = MoveGenerator::generate_pseudo_legal_moves(game_state);
    if (moves.empty()) return std::optional<ChessMove>(); 
    std::optional<ChessMove> best_move;

    // Iterative Deepening
    for (ssize_t DEPTH = min_depth; DEPTH < max_depth; DEPTH++) {
        uint16_t root_hash_move = 0;

        if (best_move.has_value()) {
            root_hash_move = best_move.value().data;
        } else {
            // Do we know this position from some other game?
            TTEntry* tt_entry = tt_cache.probe(game_state.zobrist_key);
            if (tt_entry->key == game_state.zobrist_key) {
                root_hash_move = tt_entry->move;
            }
        }

        std::vector<ScoredMove> scored_moves;
        scored_moves.reserve(moves.size());
        for (auto& move : moves) {
            bool gives_check = StateTransformer::is_king_attacked_after_move(game_state, move);
            scored_moves.push_back({
                move, 
                MoveOrdering::score_move(game_state, move, DEPTH, root_hash_move, this->killer_moves, this->history_table, gives_check)
            });
        }
        std::sort(scored_moves.begin(), scored_moves.end());

        float alpha = -std::numeric_limits<float>::infinity();
        float beta = std::numeric_limits<float>::infinity();
        
        float current_depth_best_eval = game_state.aux.get_turn() == BLACK ? std::numeric_limits<float>::infinity() : -std::numeric_limits<float>::infinity();
        std::optional<ChessMove> current_depth_best_move;

        for (ssize_t i = 0; i < scored_moves.size(); i++) {
            if (out_of_time()) {
                if (current_depth_best_move.has_value()) return current_depth_best_move;
                return best_move;
            }

            const auto& move = scored_moves[i].move;
            GameState next_state = game_state;
            if (!StateTransformer::apply_move(next_state, move)) {
                continue;
            };
            if (!current_depth_best_move.has_value()) {
                current_depth_best_move = move;
            }

            history.push_back(next_state.zobrist_key);
            float result = alpha_beta_search(next_state, history, DEPTH - 1, alpha, beta, true);
            history.pop_back();

            if (game_state.aux.get_turn() == BLACK) {
                if (result < current_depth_best_eval) {
                    current_depth_best_eval = result;
                    current_depth_best_move = move;
                }
                beta = std::min(beta, current_depth_best_eval);
            } else { 
                if (result > current_depth_best_eval) {
                    current_depth_best_eval = result;
                    current_depth_best_move = move;
                }
                alpha = std::max(alpha, current_depth_best_eval);
            }

            if (beta <= alpha) {
                break;
            }
        }
        
        best_move = current_depth_best_move;

        if (best_move.has_value()) {
            tt_cache.store(game_state.zobrist_key, DEPTH, current_depth_best_eval, best_move.value().data, TT_EXACT);
        }
    }

    return best_move;
}