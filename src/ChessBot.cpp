#include "Engine/MoveGenerator/MoveGenerator.hpp"
#include "Engine/MoveRecommender/MoveRecommender.hpp"
#include "Engine/PreparedData/PreparedData.hpp"
#include "Engine/StateTransformer/StateTransformer.hpp"
#include "Engine/Support/GameState/GameState.hpp"
#include "Tests/Benchmarks.hpp"
#include "Utils/BitsUtil.hpp"
#include <string>
#include <Bot/Bot.hpp>
#include <Interface/StudiaInterface/StudiaInterface.hpp>
#include <chrono>

// STARTUP TEST
void test()
{
    PreparedData::run_calculations();
    GameState state;
    state.print_position();

    auto moves = MoveGenerator::generate_pseudo_legal_moves(state);
    std::cout << "Legal moves count: " << moves.size() << "\n";

    BitsUtil::write_bitboard(PreparedData::king_safety_mask[WHITE][32]);
}

// BOARD TEST
void test_1()
{
    PreparedData::run_calculations();
    GameState state;
    state.load_from_fen("r1bqkbnr/pppppppp/8/8/8/4PnP1/PPPPNPRP/RNBQKB2 w Qkq - 0 1");
    state.print_position();

    auto moves = MoveGenerator::generate_pseudo_legal_moves(state);
    std::cout << "Legal moves count: " << moves.size() << "\n";
}


// PERFT TEST
void test_2()
{
    PreparedData::run_calculations();
    GameState state;
    state.load_from_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    // state.reset();
    state.print_position();
    
    int depth = 5;
    
    auto start = std::chrono::steady_clock::now();
    uint64_t total_nodes = Benchmarks::perft(state, depth);
    
    auto finish = std::chrono::steady_clock::now();
    double time_passed = std::chrono::duration<double>(finish - start).count();

    std::cout << "Depth: " << depth << "\n";
    std::cout << "Nodes: " << total_nodes << "\n";
    std::cout << "Time:  " << time_passed << " seconds\n";
    std::cout << "NPS:   " << (total_nodes / time_passed) / 1000000.0 << " Mnps\n";
}

// BOT PLAYING ITSELF TEST
void test_game()
{
    PreparedData::run_calculations();
    GameState state;
    state.reset();
    state.print_position();
    while (true) {
        auto next_move =  MoveRecommender().recommend_next_move(state, 0.2, 1, 7);
        if (!next_move.has_value()) break;
        StateTransformer::apply_move(state, *next_move);
        state.print_position();
        std::cout << "eval: " << state.evaluation_score << "\n";
        std::cout << "Move: " << state.aux.full_move_count() << "\n";
    }
}

void test_find_the_ghost() {
    PreparedData::run_calculations();
    // 1. Zrób stan startowy i wykonaj ruch a2a3
    GameState state_applied;
    state_applied.reset();
    ChessMove move_a2a3(8, 16); // Zakładam że A2 to 8, a A3 to 16
    StateTransformer::apply_move(state_applied, move_a2a3);

    // 2. Załaduj ten sam stan z czystego FEN-a
    GameState state_fen;
    // FEN po 1. a3 to: rnbqkbnr/pppppppp/8/8/8/P7/1PPPPPPP/RNBQKBNR b KQkq - 0 1
    state_fen.load_from_fen("rnbqkbnr/pppppppp/8/8/8/P7/1PPPPPPP/RNBQKBNR b KQkq - 0 1");

    // 3. Wypisz i porównaj!
    std::cout << "--- STAN Z APPLY MOVE ---\n";
    state_applied.print_position();
    
    std::cout << "--- STAN Z FEN ---\n";
    state_fen.print_position();

    // 4. Test twardych danych
    if (state_applied.total_occupancy != state_fen.total_occupancy) std::cout << "BŁĄD: total_occupancy się różni!\n";
    if (state_applied.occupancy[WHITE] != state_fen.occupancy[WHITE]) std::cout << "BŁĄD: occupancy[WHITE] się różni!\n";
    if (state_applied.occupancy[BLACK] != state_fen.occupancy[BLACK]) std::cout << "BŁĄD: occupancy[BLACK] się różni!\n";
    if (state_applied.aux.data != state_fen.aux.data) std::cout << "BŁĄD: aux sie różni!\n";
    if (state_applied.aux.can_en_passant() != state_fen.aux.can_en_passant()) std::cout << "BŁĄD: En Passant się różni!\n";
    if (state_fen.aux.can_en_passant()) std::cout << "mozna enpasdas\n";

    std::cout << "cnt: " << Benchmarks::perft(state_fen, 2) << "\n";
    std::cout << "cnt: " << Benchmarks::perft(state_applied, 2) << "\n";
}

int main(int argc, char *argv[]) 
{
    // test();
    // return -1;
    // test_game();
    // test_find_the_ghost();
    // return -1;

    int min_depth = 2;
    int max_depth = 6;
    if (argc >= 3) {
        min_depth = std::stoi(argv[1]);
        max_depth = std::stoi(argv[2]);
    } 
    PreparedData::run_calculations();
    Bot bot(min_depth, max_depth);
    StudiaInterface::run(bot);
    return 0;
}