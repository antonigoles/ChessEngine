#pragma once

#include "Engine/DynamicArena/Agent.hpp"
#include "Engine/MoveRecommender/DynamicMoveRecommender.hpp"
#include "Engine/StateTransformer/StateTransformer.hpp"
#include "Engine/Support/ChessMove.hpp"
#include "Engine/Support/GameState/GameState.hpp"
#include <algorithm>
#include <future>
#include <mutex>
#include <sys/types.h>
#include <vector>
class AgentArena
{
private:
    const ssize_t MAX_MOVES = 100;

public:
    enum GameResult
    {
        A_WON = 0,
        B_WON = 1
    };

    /*
        Return "true" if agent_a won, "false" otherwise 
    */
    GameResult perform_game(float time_per_move, const Agent& agent_a, const Agent& agent_b) 
    {
        ssize_t move_count;
        GameState game;
        game.reset();

        DynamicMoveRecommender engine_a(agent_a.alpha, (float*)agent_a.piece_values);
        DynamicMoveRecommender engine_b(agent_b.alpha, (float*)agent_b.piece_values);

        // agent_a is always white, agent_b is always black
        while (game.aux.full_move_count() <= MAX_MOVES)
        {
            std::optional<ChessMove> move;
            
            // White move
            move = engine_a.recommend_next_move(game, time_per_move);
            if (!move.has_value()) {
                return B_WON;
            }
            StateTransformer::apply_move(game, move.value());

            // Black move
            move = engine_b.recommend_next_move(game, time_per_move);
            if (!move.has_value()) {
                return A_WON;
            }
            StateTransformer::apply_move(game, move.value());
        }

        return game.eval_position() > 0.0f ? A_WON : B_WON;
    }

    class AgentTracker
    {
    public:
        Agent agent;
        ssize_t games_won;
        ssize_t games_played;
    };

    void start_simple_simulation(ssize_t agent_count, float mutation_strength, float time_for_move)
    {
        // Generate random pool of "agent_count" agents
        std::vector<AgentTracker> agents;
        Agent base_agent;
        for (int i = 0; i < agent_count; i++) {
            auto mutation = AgentMutation::get_random_mutation(mutation_strength);
            agents.push_back({base_agent.mutate(mutation), 0});
        }

        std::cerr << "Performing simulation\n";


        // Perform fights
        std::mutex mtx;
        size_t total_games = agents.size() * agents.size();
        std::atomic<size_t> current_game{0};

        unsigned int num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;

        std::vector<std::thread> workers;

        for (unsigned int t = 0; t < num_threads; ++t) {
            workers.emplace_back([&]() {
                while (true) {
                    size_t i = current_game.fetch_add(1);
                    if (i >= total_games) break;

                    size_t agent_a_index = i / agents.size();
                    size_t agent_b_index = i % agents.size();

                    if (agent_a_index == agent_b_index) continue;

                    auto result = this->perform_game(time_for_move, agents[agent_a_index].agent, agents[agent_b_index].agent);

                    std::lock_guard<std::mutex> lock(mtx);
                    if (result == A_WON) agents[agent_a_index].games_won++;
                    else agents[agent_b_index].games_won++;
                }
            });
        }

        for (auto& w : workers) {
            w.join();
        }

        std::sort(agents.begin(), agents.end(), [](const AgentTracker& a, const AgentTracker& b) {
            return a.games_won > b.games_won;
        });

        // Print results
        std::cout << "games_won,games_played,alpha,knight,bishop,rook,queen,pawn,king\n";
        for (auto agent : agents) {
            std::cout << agent.games_won << "," << agent.games_played << ","  << agent.agent.stringify() << "\n";
        }
    }

    void start_evolution(ssize_t agent_count, ssize_t epochs, float time_for_move)
    {
        // Generate random pool of "agent_count" agents
        std::vector<AgentTracker> agents;
        Agent base_agent;
        for (int i = 0; i < agent_count; i++) {
            auto mutation = AgentMutation::get_random_mutation(1.0f);
            agents.push_back({base_agent.mutate(mutation), 0});
        }

        std::cerr << "Finding viable first generation fathers\n";

        std::mutex mtx;
        size_t total_games = agents.size() * agents.size();
        std::atomic<size_t> current_game{0};

        unsigned int num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;

        std::vector<std::thread> workers;

        for (unsigned int t = 0; t < num_threads; ++t) {
            workers.emplace_back([&]() {
                while (true) {
                    size_t i = current_game.fetch_add(1);
                    if (i >= total_games) break;

                    size_t agent_a_index = i / agents.size();
                    size_t agent_b_index = i % agents.size();

                    if (agent_a_index == agent_b_index) continue;

                    auto result = this->perform_game(time_for_move, agents[agent_a_index].agent, agents[agent_b_index].agent);

                    std::lock_guard<std::mutex> lock(mtx);
                    if (result == A_WON) agents[agent_a_index].games_won++;
                    else agents[agent_b_index].games_won++;
                }
            });
        }

        for (auto& w : workers) {
            w.join();
        }

        std::sort(agents.begin(), agents.end(), [](const AgentTracker& a, const AgentTracker& b) {
            return a.games_won > b.games_won;
        });  

        std::vector<Agent> benchmark_generation;

        // Take 5 best bots from base run to get benchmark generation
        for (ssize_t i = 0; i < 5; i++) {
            benchmark_generation.push_back(agents[i].agent);
        }

        float mutation_strength = 1.0f;

        std::cerr << "Starting actual search\n";


        while (epochs > 0) {
            std::cerr << "Running next epoch\n";
            epochs--;        
            std::vector<Agent> agents_to_evolve_from;       
            for (ssize_t i = 0; i < agent_count / 20; i++) {
                agents_to_evolve_from.push_back(agents[i].agent);
            }

            // Now we want to create a new population of "agent_count" bots - include fathers in this generation
            agents.clear();
            for (auto agent_to_evolve_from : agents_to_evolve_from) {
                for (ssize_t j = 0; j < 19; j++) {
                    agents.push_back({
                        agent_to_evolve_from.mutate(AgentMutation::get_random_mutation(mutation_strength)), 0, 0
                    });
                }

                // Also push winner from previous generation so we can go back in case we only mutate to worse results
                agents.push_back({
                    agent_to_evolve_from, 0, 0
                });
            }

            size_t total_games = agents.size() * benchmark_generation.size();
            std::atomic<size_t> current_game{0};

            unsigned int num_threads = std::thread::hardware_concurrency();
            if (num_threads == 0) num_threads = 4;

            std::vector<std::thread> workers;

            for (unsigned int t = 0; t < num_threads; ++t) {
                workers.emplace_back([&]() {
                    while (true) {
                        size_t i = current_game.fetch_add(1);
                        if (i >= total_games) break;

                        size_t agent_a_index = i / benchmark_generation.size();
                        size_t agent_b_index = i % benchmark_generation.size();

                        auto result_white = this->perform_game(time_for_move, agents[agent_a_index].agent, benchmark_generation[agent_b_index]);
                        auto result_black = this->perform_game(time_for_move, benchmark_generation[agent_b_index], agents[agent_a_index].agent);

                        std::lock_guard<std::mutex> lock(mtx);
                        agents[agent_a_index].games_played+=2;
                        if (result_white == A_WON) agents[agent_a_index].games_won++;
                        if (result_black == B_WON) agents[agent_a_index].games_won++;
                    }
                });
            }

            for (auto& w : workers) {
                w.join();
            }

            std::sort(agents.begin(), agents.end(), [](const AgentTracker& a, const AgentTracker& b) {
                return a.games_won > b.games_won;
            });

            float best_score = (float)agents[0].games_won / (float)agents[0].games_played;
            std::cerr << "Finished running epoch - current best score: " << best_score << "\n";

            mutation_strength *= 0.93f;
        }

        // Return the top agent:
        std::cout << "games_won,games_played,alpha,knight,bishop,rook,queen,pawn,king\n";
        std::cout << agents[0].games_won << "," << agents[0].games_played << ","  << agents[0].agent.stringify() << "\n";
    }
};