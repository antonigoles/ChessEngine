#include "Engine/OpeningTree/OpeningTree.hpp"
#include "Engine/StateTransformer/StateTransformer.hpp"
#include "Engine/Support/ChessMove.hpp"
#include <Bot/Bot.hpp>
#include <Engine/MoveRecommender/MoveRecommender.hpp>


static double get_time_for_search(double time_for_move, double time_left)
{
    return time_left * 0.05 + time_for_move * 0.5;
}

Bot::Bot(int min_depth, int max_depth) 
    : min_depth(min_depth), 
    max_depth(max_depth) ,
    opening_tree(OpeningTree("/home/antoni/ChessBot/CerebellumLight/Cerebellum3Merge.bin"))
{
    reset();
}

void Bot::reset() {
    game_state = GameState();
    game_state.reset();
    skip_opening_tree = false;
}

std::string Bot::make_first_move(double time_for_move, double time_left) {
    this->reset();
    // auto next_move = MoveRecommender().recommend_next_move(game_state, get_time_for_search(time_for_move, time_left), min_depth, max_depth);
    auto next_move = ChessMove::from_string("e2e4"); // Start from e4
    StateTransformer::apply_move(this->game_state, next_move);
    return next_move.to_string();
}

void Bot::apply_opponent_move(const std::string& opponent_move) {
    std::stringstream ss(opponent_move);
    auto move = ChessMove::from_string(opponent_move);
    StateTransformer::apply_move(this->game_state, move);
    this->game_state.print_position();
    std::cerr << "Evaluation: (Playing as " << (this->game_state.aux.get_turn() == WHITE ? "WHITE" : "BLACK") << ")" << this->game_state.eval_position() << "\n";
}

std::string Bot::make_move(double time_for_move, double time_left, const std::string& opponent_move) {
    this->apply_opponent_move(opponent_move);
    std::optional<ChessMove> next_move;
    if (!this->skip_opening_tree) {
        
    } else {
        next_move = MoveRecommender().recommend_next_move(game_state, get_time_for_search(time_for_move, time_left), min_depth, max_depth);
    }
    StateTransformer::apply_move(this->game_state, next_move.value());
    this->game_state.print_position();
    std::cerr << "Evaluation: (Playing as " << (this->game_state.aux.get_turn() == WHITE ? "BLACK" : "WHITE") << ")" << this->game_state.eval_position() << "\n";
    return next_move.value().to_string();
}