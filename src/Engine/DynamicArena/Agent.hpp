#pragma once

#include "Engine/Support/Consts.hpp"
#include <random>
#include <string>

class AgentMutation
{
public:
    float delta_alpha = 0.0f;
    float delta_piece_values[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    static inline float random_float()
    {
        // thread local cuz maybe i want to multithread this who knows
        static thread_local std::mt19937 generator(std::random_device{}());
        static std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
        return distribution(generator);
    }

    static AgentMutation get_random_mutation(float strength)
    {
        AgentMutation mutation;

        mutation.delta_alpha += strength * random_float();
        mutation.delta_piece_values[BISHOP] += strength * random_float();
        mutation.delta_piece_values[KING] += strength * random_float();
        mutation.delta_piece_values[QUEEN] += strength * random_float();
        mutation.delta_piece_values[ROOK] += strength * random_float();
        mutation.delta_piece_values[KNIGHT] += strength * random_float();
        
        return mutation;
    }
};

class Agent
{
public:
    float alpha;
    float piece_values[6];

    std::string stringify()
    {
        std::string result = std::to_string(alpha) + ",";
        for (int i = 0; i<5; i++) {
            result += std::to_string(piece_values[i]) + ",";
        }
        result += std::to_string(piece_values[5]);
        return result;
    }

    Agent() {
        // initially agent starts with default values
        alpha = 1.0f;
        piece_values[PAWN] = 1.0f;
        piece_values[BISHOP] = 3.0f;
        piece_values[KING] = 0.0f;
        piece_values[QUEEN] = 9.0f;
        piece_values[ROOK] = 5.0f;
        piece_values[KNIGHT] = 3.0f;
    }

    Agent mutate(const AgentMutation& mutation)
    {
        Agent new_agent = *this;
        new_agent.alpha += mutation.delta_alpha;
        new_agent.piece_values[BISHOP] += mutation.delta_piece_values[BISHOP];
        new_agent.piece_values[KING] += mutation.delta_piece_values[KING];
        new_agent.piece_values[QUEEN] += mutation.delta_piece_values[QUEEN];
        new_agent.piece_values[ROOK] += mutation.delta_piece_values[ROOK];
        new_agent.piece_values[KNIGHT] += mutation.delta_piece_values[KNIGHT];
        return new_agent;
    }
};