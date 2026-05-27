#pragma once
#include <chrono>
#include <Engine/Support/ChessMove.hpp>
#include <Engine/Support/GameState/GameState.hpp>
#include <unordered_set>

class MoveRecommender
{
private:
    std::chrono::_V2::steady_clock::time_point search_start;
    double time_limit;

    std::unordered_set<uint64_t> state_cache;

public:
    MoveRecommender();

    bool out_of_time();

    void reset_round_timer();

    float alpha_beta_search(
        const GameState &state, 
        ssize_t depth,
        float alpha,
        float beta
    );

    std::optional<ChessMove> recommend_next_move(GameState &game_state, double time_for_search, int min_depth = 1, int max_depth = 4);
};