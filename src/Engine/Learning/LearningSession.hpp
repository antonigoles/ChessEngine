#pragma once
#include "Engine/Learning/LearningMutation.hpp"
#include "Engine/StateTransformer/StateTransformer.hpp"
#include "Engine/Support/Consts.hpp"
#include "Engine/Support/GameState/GameState.hpp"
#include <algorithm>
#include <string>
#include <iostream>
#include <fstream>
#include <charconv>
#include <cmath>

class LearningSession
{
private:
    struct GameStateWithResult
    {
        GameState state;
        float result;
    };

    std::vector<GameStateWithResult> learning_base;

public:
    LearningSession(const std::string& games_path, ssize_t MAX_POSITIONS) 
    {
        std::ifstream file(games_path, std::ios::binary | std::ios::ate);
        
        if (!file.is_open())
        {
            throw std::runtime_error("Could not open: " + games_path);
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> buffer(size);
        if (!file.read(buffer.data(), size))
        {
            throw std::runtime_error("Failed to read: " + games_path);
        }

        learning_base.reserve(size / 70);

        std::string_view data(buffer.data(), size);
        size_t current_pos = 0;
        int loaded_positions = 0;

        while (current_pos < data.size())
        {
            size_t end_of_line = data.find('\n', current_pos);
            if (end_of_line == std::string_view::npos) 
            {
                end_of_line = data.size();
            }

            std::string_view line = data.substr(current_pos, end_of_line - current_pos);
            
            if (!line.empty() && line.back() == '\r') 
            {
                line.remove_suffix(1);
            }

            if (!line.empty())
            {
                size_t bracket_start = line.find_last_of('[');
                size_t bracket_end = line.find_last_of(']');

                if (bracket_start != std::string_view::npos && bracket_end != std::string_view::npos && bracket_end > bracket_start)
                {
                    std::string_view fen_view = line.substr(0, bracket_start);
                    while (!fen_view.empty() && std::isspace(fen_view.back())) 
                    {
                        fen_view.remove_suffix(1);
                    }

                    std::string_view result_view = line.substr(bracket_start + 1, bracket_end - bracket_start - 1);
                    float result = 0.0f;
                    
                    auto [ptr, ec] = std::from_chars(result_view.data(), result_view.data() + result_view.size(), result);

                    if (ec == std::errc())
                    {
                        GameState state;
                        state.load_from_fen(std::string(fen_view));
                        StateTransformer::hard_eval_refresh(state);
                        learning_base.push_back({state, result});
                        loaded_positions++;
                        if (loaded_positions > MAX_POSITIONS) {
                            break;
                        }
                    }
                    else
                    {
                        std::cerr << "Skipping mangled line: " << line << std::endl;
                    }
                }
                else
                {
                    std::cerr << "Skipping mangled line: " << line << std::endl;
                }
            }
            current_pos = end_of_line + 1;
        }
        file.close();
        std::cout << "Loaded " << loaded_positions << " positions to learn from.\n";
    }

    const double K = 400.0;

    double sigmoid(double eval) const
    {
        return 1.0 / (1.0 + std::pow(10.0, -eval / K));
    }

    double calculate_mse(const LearningMutation& state, size_t start_idx, size_t batch_size)
    {
        double error_sum = 0.0;
        
        for (size_t i = start_idx; i < start_idx + batch_size; ++i)
        {
            const GameState& game = learning_base[i].state;
            
            double score = state.eval(game);
            double sigmoid = 1.0 / (1.0 + std::pow(10.0, -score / K));
            double expected = learning_base[i].result;
            double error = expected - sigmoid;
            error_sum += error * error;
        }
        
        return error_sum / batch_size;
    }

    void export_tuned_pst(const LearningMutation& final_state)
    {
        const std::string piece_names[] = {"knight", "bishop", "rook", "queen", "pawn", "king"};
        
        for (int piece = 0; piece <= 5; ++piece)
        {
            float base_val_mg = pst_mg_value[piece]; 
            
            std::cout << "inline const int64_t mg_" << piece_names[piece] << "_table[64] = {\n";
            
            for (int sq = 0; sq < 64; ++sq)
            {
                if (sq % 8 == 0) std::cout << "    "; 
                float pst_bonus = final_state.mg_pst[piece][sq] - base_val_mg;
                std::cout << ((int64_t)std::round(pst_bonus)) << ", ";
                if ((sq + 1) % 8 == 0) std::cout << "\n";
            }
            std::cout << "};\n\n";
        }

        for (int piece = 0; piece <= 5; ++piece)
        {
            float base_val_eg = pst_eg_value[piece]; 
            
            std::cout << "inline const int64_t eg_" << piece_names[piece] << "_table[64] = {\n";
            
            for (int sq = 0; sq < 64; ++sq)
            {
                if (sq % 8 == 0) std::cout << "    "; 
                float pst_bonus = final_state.eg_pst[piece][sq] - base_val_eg;
                std::cout << ((int64_t)std::round(pst_bonus)) << ", ";
                if ((sq + 1) % 8 == 0) std::cout << "\n";
            }
            std::cout << "};\n\n";
        }
    }

    void run_spsa_tuning()
    {
        std::cout << "Starting SPSA..." << std::endl;

        LearningMutation current_state;
        current_state.init_with_local_pst();
        double best_mse = calculate_mse(current_state, 0, learning_base.size());
        std::cout << "Starting MSE: " << best_mse << "\n\n";

        const double a = 1500000.0;       // Starting max step
        const double c = 20.0;      // starting noise
        const double A = 5000.0;    // a_k half life
        const double alpha = 0.602;
        const double gamma = 0.101; 

        const int MAX_ITERATIONS = 10000; // Epoch count

        const size_t TOTAL_DATA = learning_base.size();
        const size_t BATCH_SIZE = 100000;
        size_t current_batch_start = 0;
        std::mt19937 rng(std::random_device{}());
        std::shuffle(learning_base.begin(), learning_base.end(), rng);

        for (int k = 1; k <= MAX_ITERATIONS; ++k)
        {
            float a_k = static_cast<float>(a / std::pow(A + k, alpha));
            float c_k = static_cast<float>(c / std::pow(k, gamma));

            LearningMutation delta;
            delta.randomize_pst_delta();

            LearningMutation state_plus = current_state.get_perturbed_pst_state(delta, c_k, true);
            LearningMutation state_minus = current_state.get_perturbed_pst_state(delta, c_k, false);

            double mse_plus = calculate_mse(state_plus, current_batch_start, BATCH_SIZE);
            double mse_minus = calculate_mse(state_minus, current_batch_start, BATCH_SIZE);

            current_batch_start += BATCH_SIZE;

            if (current_batch_start + BATCH_SIZE >= TOTAL_DATA)
            {
                std::shuffle(learning_base.begin(), learning_base.end(), rng);
                current_batch_start = 0;
            }

            float gradient_base = static_cast<float>((mse_plus - mse_minus) / (2.0 * c_k));

            current_state.update_weights(delta, gradient_base, a_k);

            const GameState& test_game = learning_base[0].state;

            if (k % 100 == 0)
            {
                double current_mse = calculate_mse(current_state, 0, learning_base.size());
                std::cout << "Epoch " << k << "/" << MAX_ITERATIONS 
                        << " | Step (a_k): " << a_k 
                        << " | Noise (c_k): " << c_k 
                        << " | Current MSE: " << current_mse << std::endl;
            }
        }

        std::cout << "\nSPSA finished" << std::endl;
        export_tuned_pst(current_state);
    }

    void perform_evolution(LearningMutation& mutation)
    {
        mutation.init_with_local_pst();
        double starting_mse = calculate_mse(mutation, 0, learning_base.size());
        double current_mse = starting_mse;
        bool improved = true;
        int iteration = 1;
        float step = 0.1f;
        while (improved) {
            improved = false;
            for (int i = 0; i<LearningMutation::PARAMS_COUNT; i++) {
                if (mutation.should_skip_mutaion[i]) {
                    continue;
                }
                float original_value = mutation.weights[i];
                mutation.weights[i] = original_value + step;
                double mse_up = calculate_mse(mutation, 0, learning_base.size());

                if (mse_up < current_mse)
                {
                    current_mse = mse_up;
                    improved = true;
                    std::cout << "Parametr " << i << " increased to " << mutation.weights[i] << " (MSE: " << current_mse << ")\n";
                    continue;
                }

                // if this did not improve, let's take a step back instead
                mutation.weights[i] = original_value - step;
                double mse_down = calculate_mse(mutation, 0, learning_base.size());

                if (mse_down < current_mse)
                {
                    // We don't want to fall below 0
                    if (mutation.weights[i] >= 0.0f) 
                    {
                        current_mse = mse_down;
                        improved = true;
                        std::cout << "Parametr " << i << " decreased to " << mutation.weights[i] << " (MSE: " << current_mse << ")\n";
                        continue;
                    }
                }

                mutation.weights[i] = original_value;
            }

            iteration++;

            if (!improved && step > 0.00001f) 
            {
                step /= 2.0f;
                improved = true;
                std::cout << "Decreasing mutation step to: " << step << "\n";
            }
        }

        std::cout << "Tuning finished, final MSE: " << current_mse << " started from: " << starting_mse << "\n";
    }   
};