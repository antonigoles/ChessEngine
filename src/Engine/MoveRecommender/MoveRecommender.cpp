#include "Engine/MoveGenerator/MoveGenerator.hpp"
#include "Engine/StateTransformer/StateTransformer.hpp"
#include "Engine/Support/Consts.hpp"
#include <Engine/MoveRecommender/MoveRecommender.hpp>
#include <optional>
#include <sys/types.h>

MoveRecommender::MoveRecommender() : time_limit(0.0) {
    reset_round_timer();
};

void MoveRecommender::reset_round_timer()
{
    search_start = std::chrono::steady_clock::now();
}

bool MoveRecommender::out_of_time()
{
    auto now = std::chrono::steady_clock::now();
    auto time_passed = std::chrono::duration<double>(now - this->search_start).count();
    if (time_passed >= this->time_limit * 0.93) {
        return true;
    }
    return false;
}

float MoveRecommender::alpha_beta_search(
    const GameState &state, 
    ssize_t depth,
    float alpha,
    float beta
) {
    auto moves = MoveGenerator::generate_pseudo_legal_moves(state);

    // The game is lost
    if (moves.empty() && state.is_checked) {
        return state.aux.get_turn() == WHITE ? -10000000.0f : 10000000.0f;
    }

    // Draw when I'm so ahead is bad
    if (moves.empty() && !state.is_checked) {
        return state.aux.get_turn() == WHITE && state.eval_position() > 900 
            ? -10000000.0f
            : state.aux.get_turn() == BLACK && state.eval_position() < -900 ? 10000000.0f 
            : 0;
    }

    if (depth == 0 || this->out_of_time() ) {
        return state.eval_position();
    }

    float best_eval = state.aux.get_turn() == BLACK ? std::numeric_limits<float>::infinity() : -std::numeric_limits<float>::infinity();

    for (const auto& move : moves) {
        GameState next_state = state;
        if (!StateTransformer::apply_move(next_state, move)) {
            continue;
        };
        
        float eval = alpha_beta_search(next_state, depth - 1, alpha, beta);
        if (this->out_of_time()) return best_eval;

        if (state.aux.get_turn() == BLACK) {
            best_eval = std::min(best_eval, eval);
            beta = std::min(beta, eval);
        } else {
            best_eval = std::max(best_eval, eval);
            alpha = std::max(alpha, eval);
        }

        if (beta <= alpha) {
            break; 
        }
    }

    return best_eval;
}

std::optional<ChessMove> MoveRecommender::recommend_next_move(GameState &game_state, double time_for_search, int min_depth, int max_depth) 
{
    this->time_limit = time_for_search;
    
    auto moves = MoveGenerator::generate_pseudo_legal_moves(game_state);

    // for (auto m : moves) {
    //     std::cerr << m.to_string() << ", ";
    // }
    // std::cerr << "\n";

    if (moves.empty()) return std::optional<ChessMove>(); 

    // Make positions out of moves
    std::vector<std::pair<ChessMove, GameState>> positions;
    for (auto& move : moves) {
        GameState next_state = game_state;
        if (!StateTransformer::apply_move(next_state, move)) {
            continue;
        };
        positions.push_back({move, next_state});
    }

    std::optional<ChessMove> best_move = positions[0].first;

    // std::sort(
    //     positions.begin(), positions.end(), 
    //     [](const std::pair<ChessMove, GameState>& a, const std::pair<ChessMove, GameState>& b) {
    //         return a.second.evaluation_score < b.second.evaluation_score;
    //     }
    // );    
    
    // Iterative Deepening
    for (ssize_t DEPTH = min_depth; DEPTH < max_depth; DEPTH++) {
        float alpha = -std::numeric_limits<float>::infinity();
        float beta = std::numeric_limits<float>::infinity();
        
        float current_depth_best_eval = game_state.aux.get_turn() == BLACK ? std::numeric_limits<float>::infinity() : -std::numeric_limits<float>::infinity();
        ChessMove current_depth_best_move = positions[0].first;

        for (ssize_t i = 0; i < positions.size(); i++) {
            if (out_of_time()) return best_move;

            auto& position = positions[i];

            GameState next_state = position.second;
            float result = alpha_beta_search(next_state, DEPTH - 1, alpha, beta);

            if (game_state.aux.get_turn() == BLACK) {
                if (result < current_depth_best_eval) {
                    current_depth_best_eval = result;
                    current_depth_best_move = position.first;
                }
                beta = std::min(beta, current_depth_best_eval);
            } else { 
                if (result > current_depth_best_eval) {
                    current_depth_best_eval = result;
                    current_depth_best_move = position.first;
                }
                alpha = std::max(alpha, current_depth_best_eval);
            }
        }
        
        best_move = current_depth_best_move;
    }
    
    return best_move;
}