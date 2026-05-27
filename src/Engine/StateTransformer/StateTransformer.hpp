#pragma once
#include <Engine/Support/GameState/GameState.hpp>
#include <Engine/Support/ChessMove.hpp>

class StateTransformer
{
public:
    static bool apply_move(GameState& state, const ChessMove& move);
};
