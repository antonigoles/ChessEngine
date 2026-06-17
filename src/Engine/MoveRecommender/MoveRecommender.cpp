#include "Engine/MoveGenerator/MoveGenerator.hpp"
#include "Engine/MoveRecommender/MoveOrdering.hpp"
#include "Engine/MoveRecommender/TTCache.hpp"
#include "Engine/StateTransformer/StateTransformer.hpp"
#include "Engine/Support/ChessMove.hpp"
#include "Engine/Support/Consts.hpp"
#include <Engine/MoveRecommender/MoveRecommender.hpp>
#include <cmath>
#include <optional>
#include <sys/types.h>
#include <Engine/MoveRecommender/MCTSNode.hpp>

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

std::optional<ChessMove> MoveRecommender::recommend_next_move(
    GameState &game_state, 
    double time_for_search
) {
    this->time_limit = time_for_search;
    this->reset_round_timer();
    this->visited_nodes = 0;

    MCTSNode root(ChessMove(), nullptr, game_state);

    if (root.untried_moves.empty()) {
        return std::nullopt;
    }

    const double C = 1.41;

    // Main MCTS loop - we're building the tree until we have the time
    while (!this->out_of_time()) {
        this->visited_nodes++;
        
        MCTSNode* node = &root;
        GameState current_state = game_state;

        // 1. Selection
        while (node->untried_moves.empty() && !node->children.empty()) {
            MCTSNode* best_child = nullptr;
            double best_uct = -std::numeric_limits<double>::infinity();

            for (MCTSNode* child : node->children) {
                double exploit = child->white_score_sum / child->visits;
                if (node->turn == BLACK) {
                    exploit = 1.0 - exploit; // We "Reverse" this value for the other color
                }

                // UCT
                double explore = C * std::sqrt(std::log(node->visits) / child->visits);
                double uct = exploit + explore;

                if (uct > best_uct) {
                    best_uct = uct;
                    best_child = child;
                }
            }
            
            node = best_child;
            StateTransformer::apply_move(current_state, node->move);
        }

        // Expansion
        if (!node->untried_moves.empty()) {
            // Insertion sort is appereantly optimal here
            ChessMove move_to_try = node->untried_moves.back().move;
            node->untried_moves.pop_back();

            StateTransformer::apply_move(current_state, move_to_try);
            MCTSNode* new_node = new MCTSNode(move_to_try, node, current_state);
            node->children.push_back(new_node);
            node = new_node;
        }

        // Simulation (quiescence_search)
        float eval;
        if (current_state.is_checked && MoveGenerator::generate_pseudo_legal_moves(current_state).empty()) {
            // Mate
            eval = (current_state.aux.get_turn() == WHITE) ? -100000.0f : 100000.0f;
        } else {
            // infinite window quiescence_search
            eval = quiescence_search(current_state, -1000000.0f, 1000000.0f);
        }

        // Sigmoid win probability evaluation (same K as in the LearningSession algorithm)
        double white_win_probability = 1.0 / (1.0 + std::exp(-eval / 480.0));

        // Backpropagation
        while (node != nullptr) {
            node->visits++;
            node->white_score_sum += white_win_probability;
            node = node->parent;
        }
    }

    // Return the most visited move
    MCTSNode* best_child = nullptr;
    int max_visits = -1;

    for (MCTSNode* child : root.children) {
        if (child->visits > max_visits) {
            max_visits = child->visits;
            best_child = child;
        }
    }

    if (best_child) {
        return best_child->move;
    }
    
    return std::nullopt;
}