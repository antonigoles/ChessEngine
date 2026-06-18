#include "Engine/MoveGenerator/MoveGenerator.hpp"
#include "Engine/MoveRecommender/MoveOrdering.hpp"
#include "Engine/StateTransformer/StateTransformer.hpp"
#include "Engine/Support/ChessMove.hpp"
#include "Engine/Support/GameState/GameState.hpp"
#include <algorithm>
#include <vector>
#include <cmath>

class MCTSNode {
public:
    class RatedChessMove 
    {
    public:
        int64_t rating;
        ChessMove move;
    };

    ChessMove move;
    MCTSNode* parent; 
    std::vector<MCTSNode*> children; 
    std::vector<RatedChessMove> untried_moves;
    
    Color turn;
    int visits;
    double white_score_sum;

    MCTSNode(ChessMove m, MCTSNode* p, GameState& state) 
        : move(m), parent(p), turn(state.aux.get_turn()), visits(0), white_score_sum(0.0) 
    {
        auto pseudo_moves = MoveGenerator::generate_pseudo_legal_moves(state);
        
        // Filter out illegal moves to make expansion simpler
        for (const auto& pm : pseudo_moves) {
            GameState test_state = state;
            if (StateTransformer::apply_move(test_state, pm)) {
                RatedChessMove rm = {
                    MoveOrdering::score_move(state, pm, StateTransformer::is_king_attacked_after_move(state, pm)), 
                    pm
                };
                untried_moves.push_back(rm);
            }
        }

        std::sort(untried_moves.begin(), untried_moves.end(), [](const RatedChessMove& a, const RatedChessMove& b) {
            return a.rating  < b.rating;
        });
    }

    ~MCTSNode() {
        for (auto child : children) {
            delete child;
        }
    }
};