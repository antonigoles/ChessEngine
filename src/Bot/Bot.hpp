#pragma once
#include "Engine/OpeningTree/OpeningTree.hpp"
#include <string>
#include <Engine/Support/GameState/GameState.hpp>

class Bot {
public:
    bool skip_opening_tree = false;
    OpeningTree opening_tree;
    GameState game_state;
    bool is_black = false;
    int min_depth = 1;
    int max_depth = 8;

    Bot(int min_depth, int max_depth);

    void reset();

    std::string make_first_move(double time_for_move, double time_left);

    void apply_opponent_move(const std::string& opponent_move);

    std::string make_move(double time_for_move, double time_left, const std::string& opponent_move);
};