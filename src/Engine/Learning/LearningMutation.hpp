#pragma once
#include "Engine/PreparedData/PreparedData.hpp"
#include "Engine/Support/Consts.hpp"
#include "Engine/Support/GameState/GameState.hpp"
#include "Utils/MathUtil.hpp"
#include <cmath>
#include <cstring>
class LearningMutation
{
public:
    static constexpr int PARAMS_COUNT = 13;

    bool should_skip_mutaion[PARAMS_COUNT] = {
        false,
        false,
        false,
        false,
        false,
        false,
        true,
        false,
        true,
        true,
        false,
        false,
        true,
    };

    float weights[PARAMS_COUNT] = {
        DOUBLED_PAWNS_SCORE_MULTIPLIER,
        ISOLATED_PAWNS_SCORE_MULTIPLIER,
        SEMI_ISOLATED_PAWNS_SCORE_MULTIPLIER,
        PASSED_PAWN_STRUCTURE_SCORE_MULTIPLIER,
        KING_PAWNS_SCORE_MULTIPLIER,
        KING_ATTACKERS_SCORE_MULTIPLIER,
        PST_SCORE_MULTIPLIER,
        MOBILITY_SCORE_MULTIPLIER,
        MID_GAME_DEVELOPMENT_SCORE_MULTIPLIER,
        EARLY_GAME_DEVELOPMENT_SCORE_MULTIPLIER,
        ACTIVE_PIECE_MULTIPLIER,
        SPACE_ADVANTAGE_MULTIPLIER,
        SIMPLIFICATION_BONUS_MULTIPLIER
    };

    float mg_pst[6][64];
    float eg_pst[6][64];

    void init_with_local_pst()
    {
        for (int piece = 0; piece <= 5; ++piece)
        {
            for (int sq = 0; sq < 64; ++sq)
            {
                mg_pst[piece][sq] = PreparedData::mg_pst_table[WHITE][piece][sq];
                eg_pst[piece][sq] = PreparedData::eg_pst_table[WHITE][piece][sq];
            }
        }
    }

    void print_mutation()
    {
        std::cout << "inline constexpr float DOUBLED_PAWNS_SCORE_MULTIPLIER = " << weights[0] << "f;\n";
        std::cout << "inline constexpr float ISOLATED_PAWNS_SCORE_MULTIPLIER = " << weights[1] << "f;\n";
        std::cout << "inline constexpr float SEMI_ISOLATED_PAWNS_SCORE_MULTIPLIER = " << weights[2] << "f;\n";
        std::cout << "inline constexpr float PASSED_PAWN_STRUCTURE_SCORE_MULTIPLIER = " << weights[3] << "f;\n";
        std::cout << "\n";
        std::cout << "inline constexpr float KING_PAWNS_SCORE_MULTIPLIER = " << weights[4] << "f;\n";
        std::cout << "inline constexpr float KING_ATTACKERS_SCORE_MULTIPLIER = " << weights[5] << "f;\n";
        std::cout << "\n";
        std::cout << "inline constexpr float PST_SCORE_MULTIPLIER = " << weights[6] << "f;\n";
        std::cout << "\n";
        std::cout << "inline constexpr float MOBILITY_SCORE_MULTIPLIER = " << weights[7] << "f;\n";
        std::cout << "\n";
        std::cout << "inline constexpr float MID_GAME_DEVELOPMENT_SCORE_MULTIPLIER =" << weights[8] << "f;\n";
        std::cout << "inline constexpr float EARLY_GAME_DEVELOPMENT_SCORE_MULTIPLIER = " << weights[9] << "f;\n";
        std::cout << "\n";
        std::cout << "inline constexpr float ACTIVE_PIECE_MULTIPLIER = " << weights[10] << "f;\n";
        std::cout << "inline constexpr float SPACE_ADVANTAGE_MULTIPLIER = " << weights[11] << "f;\n";
        std::cout << "\n";
        std::cout << "inline constexpr float SIMPLIFICATION_BONUS_MULTIPLIER = " << weights[12] << "f;\n";
    }

    float get_pst_score(const GameState& game) const 
    {
        float mid_game_score = 0.0f;
        float end_game_score = 0.0f;
        float game_phase = 0.0f;

        for (int color = BLACK; color <= WHITE; ++color) 
        {
            for (int piece = 0; piece <= 5; ++piece) 
            {
                uint64_t bb = game.bitboards[color][piece];
                while (bb) 
                {
                    int sq = std::countr_zero(bb);

                    int relative_sq = (color == BLACK) ? (sq ^ 56) : sq;

                    float mg_val = mg_pst[piece][relative_sq];

                    float eg_val = eg_pst[piece][relative_sq];

                    if (color == WHITE) 
                    {
                        mid_game_score += mg_val;
                        end_game_score += eg_val;
                    } 
                    else 
                    {
                        mid_game_score -= mg_val;
                        end_game_score -= eg_val;
                    }

                    bb &= bb - 1;

                    game_phase += gamephase_inc[piece];
                }
            }
        }

        game_phase = game_phase > 24.0f ? 24.0f : game_phase;
        return (mid_game_score * game_phase + end_game_score * (24.0f - game_phase)) / 24.0f;
    }

    float eval(const GameState& game) const
    {
        auto phased = game.get_phase_scaled();

        return 
            game.pawn_structure_score.doubled_pawns * weights[0]
            + game.pawn_structure_score.isolated_pawns * weights[1]
            + game.pawn_structure_score.semi_isolated_pawns * weights[2]
            + game.pawn_structure_score.passed_pawns * weights[3]

            + phased.king_pawns * weights[4] 
            + phased.king_attackers * weights[5] 

            + get_pst_score(game) * weights[6]

            + game.safe_mobility_score * weights[7]

            + phased.midgame_development * weights[8]
            + phased.earlygame_development * weights[9]

            + game.active_piece_score * weights[10]
            + game.space_advantage_score * weights[11]

            + phased.simplification_bonus * weights[12];
    }

    void randomize_pst_delta()
    {
        for (int piece = 0; piece <= 5; ++piece)
        {
            for (int sq_left = 0; sq_left < 64; ++sq_left)
            {
                if (sq_left % 8 < 4) 
                {
                    float d_mg = (MathUtil::random() > 0.5f) ? 1.0f : -1.0f;
                    float d_eg = (MathUtil::random() > 0.5f) ? 1.0f : -1.0f;

                    int sq_right = sq_left ^ 7;

                    mg_pst[piece][sq_left] = d_mg;
                    mg_pst[piece][sq_right] = d_mg;

                    eg_pst[piece][sq_left] = d_eg;
                    eg_pst[piece][sq_right] = d_eg;
                }
            }
        }
    }

    LearningMutation get_perturbed_pst_state(const LearningMutation& delta, float c_k, bool is_plus) const
    {
        LearningMutation result;

        std::memcpy(result.weights, this->weights, sizeof(this->weights));
        std::memcpy(result.mg_pst, this->mg_pst, sizeof(this->mg_pst));
        std::memcpy(result.eg_pst, this->eg_pst, sizeof(this->eg_pst));

        float sign = is_plus ? 1.0f : -1.0f;

        for (int p = 0; p < 6; ++p)
        {
            for (int s = 0; s < 64; ++s)
            {
                result.mg_pst[p][s] += sign * c_k * delta.mg_pst[p][s];
                result.eg_pst[p][s] += sign * c_k * delta.eg_pst[p][s];
            }
        }
        return result;
    }

    void update_weights(const LearningMutation& delta, float gradient_base, float a_k)
    {
        for (int p = 0; p < 6; ++p)
        {
            for (int s = 0; s < 64; ++s)
            {
                if (s % 8 < 4)
                {
                    float grad_mg = gradient_base * delta.mg_pst[p][s];
                    float grad_eg = gradient_base * delta.eg_pst[p][s];

                    mg_pst[p][s] -= a_k * grad_mg;
                    eg_pst[p][s] -= a_k * grad_eg;

                    mg_pst[p][s ^ 7] = mg_pst[p][s];
                    eg_pst[p][s ^ 7] = eg_pst[p][s];
                }
            }
        }
    }
    
    LearningMutation mutate(float base = 0.005f)
    {
        LearningMutation mutation = *this;
        for (ssize_t i = 0; i<PARAMS_COUNT; i++) {
            mutation.weights[i] += base * (MathUtil::random() * 2.0f - 1.0f);
        }
        return mutation;
    }
};