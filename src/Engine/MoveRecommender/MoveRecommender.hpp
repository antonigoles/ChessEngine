#pragma once
#include "Engine/MoveRecommender/TTCache.hpp"
#include <chrono>
#include <Engine/Support/ChessMove.hpp>
#include <Engine/Support/GameState/GameState.hpp>

class MoveRecommender
{
private:
    std::chrono::_V2::steady_clock::time_point search_start;
    double time_limit;

    TTCache tt_cache;

    ChessMove killer_moves[64][2];

    // Format: [Color][From][To]
    int32_t history_table[2][64][64];

    uint64_t visited_nodes = 0;

public:
    MoveRecommender();

    bool out_of_time();

    void reset_round_timer();

    float quiescence_search(const GameState& state, float alpha, float beta);

    float alpha_beta_search(
        const GameState &state,
        std::vector<uint64_t>& history,
        ssize_t depth,
        float alpha,
        float beta,
        bool allow_null
    );

    std::optional<ChessMove> recommend_next_move(
        GameState &game_state, 
        double time_for_search, 
        std::vector<uint64_t>& history,
        int min_depth = 1, 
        int max_depth = 4
    );

    void clear_heuristics() {
        for (int i = 0; i < 64; i++) {
            killer_moves[i][0] = ChessMove();
            killer_moves[i][1] = ChessMove();
        }
        for (int c = 0; c < 2; c++)
            for (int from = 0; from < 64; from++)
                for (int to = 0; to < 64; to++)
                    history_table[c][from][to] = 0;
    }
};