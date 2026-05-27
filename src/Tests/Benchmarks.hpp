#pragma once
#include "Engine/StateTransformer/StateTransformer.hpp"
#include <cstdint>
#include <Engine/Support/GameState/GameState.hpp>
#include <Engine/MoveGenerator/MoveGenerator.hpp>

class Benchmarks {
public:
    static uint64_t perft(const GameState& state, int depth) {
        if (depth == 0) {
            return 1ULL;
        }

        auto moves = MoveGenerator::generate_pseudo_legal_moves(state);

        uint64_t nodes = 0;

        for (const auto& move : moves) {
            GameState next_state = state; 
            
            bool result = StateTransformer::apply_move(next_state, move);

            uint64_t cnt = result ? Benchmarks::perft(next_state, depth - 1) : 0;
            
            nodes += cnt;
        }

        return nodes;
    }
};