#pragma once
#include <Engine/Support/GameState/GameState.hpp>
#include <Engine/Support/ChessMove.hpp>

class StateTransformer
{
public:
    static bool is_king_attacked_after_move(const GameState& state, const ChessMove& move);
    static bool apply_move(GameState& state, const ChessMove& move);
};
