#include "Engine/PreparedData/PreparedData.hpp"
#include <Engine/Support/GameState/GameState.hpp>
#include <Engine/PreparedData/Zobrist.hpp>

void GameState::reset()
{
    this->load_from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    zobrist_key = 0x463b96181691fc9c;

    // Calculate initial PST evaluation
    this->mid_game_score[WHITE] = 0; this->mid_game_score[BLACK] = 0;
    this->end_game_score[WHITE] = 0; this->end_game_score[BLACK] = 0;
    this->game_phase = 0;

    for (int c = 0; c <= 1; ++c) {
        for (int p = 0; p < 6; ++p) {
            uint64_t bb = this->bitboards[c][p];
            while (bb) {
                int sq = std::countr_zero(bb);
                bb &= (bb - 1); 
                
                this->mid_game_score[c] += PreparedData::mg_pst_table[c][p][sq];
                this->end_game_score[c] += PreparedData::eg_pst_table[c][p][sq];
                this->game_phase += gamephase_inc[p];
            }
        }
    }
}