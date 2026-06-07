#pragma once
#include "Engine/Support/Consts.hpp"
#include "Utils/MathUtil.hpp"
#include "Utils/StringUtils.hpp"
#include <cassert>
#include <string>
#include <Engine/Support/GameState/GameStateAux.hpp>
#include <unordered_map>
#include <iostream>

class GameState {
public:
    float simplification_bonus = 0.0f;

    struct pawn_structure_score 
    {
        float isolated_pawns = 0.0f;
        float semi_isolated_pawns = 0.0f;
        float passed_pawns = 0.0f;
        float doubled_pawns = 0.0f;
    };

    pawn_structure_score pawn_structure_score;

    struct king_safety_score 
    {
        float pawns_in_front = 0.0f;
        float attackers = 0.0f;
    };

    king_safety_score king_safety_score;

    float early_game_development_score = 0.0f;
    float mid_game_development_score = 0.0f;
    float development_score = 0.0f;
    
    float safe_mobility_score = 0.0f;

    float active_piece_score = 0.0f;

    float space_advantage_score = 0.0f;

    float mid_game_score[2] = {0.0f, 0.0f}; 
    float end_game_score[2] = {0.0f, 0.0f}; 
    int64_t game_phase = 0;

    float pst_score = 0.0f;

    bool is_checked = false;

    uint64_t zobrist_key;

    // Base BitBoards 
    // 0 - black
    // 1 - white
    uint64_t bitboards[2][6];
    uint64_t occupancy[2];
    uint64_t total_occupancy;
    GameStateAux aux; 

    GameState() { 
        bitboards[WHITE][ChessPiece::BISHOP] = 0;
        bitboards[WHITE][ChessPiece::KING] = 0;
        bitboards[WHITE][ChessPiece::KNIGHT] = 0;
        bitboards[WHITE][ChessPiece::PAWN] = 0;
        bitboards[WHITE][ChessPiece::ROOK] = 0;
        bitboards[WHITE][ChessPiece::QUEEN] = 0;

        bitboards[BLACK][ChessPiece::BISHOP] = 0;
        bitboards[BLACK][ChessPiece::KING] = 0;
        bitboards[BLACK][ChessPiece::KNIGHT] = 0;
        bitboards[BLACK][ChessPiece::PAWN] = 0;
        bitboards[BLACK][ChessPiece::ROOK] = 0;
        bitboards[BLACK][ChessPiece::QUEEN] = 0;

        occupancy[WHITE] = 0;
        occupancy[BLACK] = 0;

        total_occupancy = 0;

        aux.data = 0;

        zobrist_key = 0;
    }

    void reset();

    struct PhaseScaledHeuristics 
    {
        float pst_score = 0.0f;
        float king_pawns = 0.0f;
        float king_attackers = 0.0f;
        float midgame_development = 0.0f;
        float earlygame_development = 0.0f;
        float simplification_bonus = 0.0f;
    };

    PhaseScaledHeuristics get_phase_scaled() const
    {
        PhaseScaledHeuristics result;

        int64_t mg_score_delta = this->mid_game_score[WHITE] - this->mid_game_score[BLACK]; 
        int64_t eg_score_delta = this->end_game_score[WHITE] - this->end_game_score[BLACK]; 

        int64_t mg_phase = this->game_phase > 24 ? 24 : this->game_phase; 
        int64_t eg_phase = 24 - mg_phase;

        int64_t final_pst_score = (mg_score_delta * mg_phase + eg_score_delta * eg_phase) / 24;
        result.pst_score = static_cast<float>(final_pst_score); 


        // MID GAME only heuristics
        float mid_game_phase_weight = MathUtil::clamp(static_cast<float>(this->game_phase - 12) / 12.0f, 0.0f, 1.0f);
        float early_game_phase_weight = MathUtil::clamp(static_cast<float>(this->game_phase - 20) / 4.0f, 0.0f, 1.0f);

        result.king_pawns = mid_game_phase_weight * this->king_safety_score.pawns_in_front;
        result.king_attackers = this->king_safety_score.attackers;

        result.earlygame_development = this->mid_game_development_score * early_game_phase_weight;
        result.midgame_development = this->mid_game_development_score * mid_game_phase_weight;

        // SIMPLIFICATION BONUS
        result.simplification_bonus = this->pst_score * (float)eg_phase / 24.0f;

        return result;
    }

    float eval_position() const
    {
        auto phased = this->get_phase_scaled();

        return 
            this->pawn_structure_score.doubled_pawns * DOUBLED_PAWNS_SCORE_MULTIPLIER
            + this->pawn_structure_score.isolated_pawns * ISOLATED_PAWNS_SCORE_MULTIPLIER
            + this->pawn_structure_score.semi_isolated_pawns * SEMI_ISOLATED_PAWNS_SCORE_MULTIPLIER
            + this->pawn_structure_score.passed_pawns * PASSED_PAWN_STRUCTURE_SCORE_MULTIPLIER

            + phased.king_pawns * KING_PAWNS_SCORE_MULTIPLIER 
            + phased.king_attackers * KING_ATTACKERS_SCORE_MULTIPLIER 

            + phased.pst_score * PST_SCORE_MULTIPLIER

            + safe_mobility_score * MOBILITY_SCORE_MULTIPLIER

            + phased.earlygame_development * MID_GAME_DEVELOPMENT_SCORE_MULTIPLIER
            + phased.midgame_development * EARLY_GAME_DEVELOPMENT_SCORE_MULTIPLIER

            + active_piece_score * ACTIVE_PIECE_MULTIPLIER
            + space_advantage_score * SPACE_ADVANTAGE_MULTIPLIER

            + phased.simplification_bonus * SIMPLIFICATION_BONUS_MULTIPLIER;
    }

    GameState clone() 
    {
        return *this;
    }

    void print_position()
    {
        std::unordered_map<Color, std::unordered_map<ChessPiece, std::string>> piece_map;

        piece_map[BLACK][PAWN] = "♙";
        piece_map[BLACK][KING] = "♔";
        piece_map[BLACK][ROOK] = "♖";
        piece_map[BLACK][QUEEN] = "♕";
        piece_map[BLACK][KNIGHT] = "♘";
        piece_map[BLACK][BISHOP] = "♗";

        piece_map[WHITE][PAWN] = "♟";
        piece_map[WHITE][KING] = "♚";
        piece_map[WHITE][ROOK] = "♜";
        piece_map[WHITE][QUEEN] = "♛";
        piece_map[WHITE][KNIGHT] = "♞";
        piece_map[WHITE][BISHOP] = "♝";

        std::cerr << "  ABCDEFGH\n";
        for (int64_t y = 7; y >= 0; y--) {
            std::cerr << y+1 << " ";
            for (int64_t x = 0; x <= 7; x++) {
                uint64_t shift = 8 * y + x;
               
                bool empty = true;

                for (uint8_t piece_code = 0; piece_code <= 0x5; piece_code++) {
                    auto piece = (ChessPiece)(piece_code);
                    uint64_t mask = (1ULL << shift);

                    if (this->bitboards[WHITE][piece] & mask) {
                        std::cerr << piece_map[WHITE][piece];
                        empty = false;
                    } else if (this->bitboards[BLACK][piece] & mask) {
                        std::cerr << piece_map[BLACK][piece];
                        empty = false;
                    }
                }

                if (empty) {
                    std::cerr << "·";
                }
            }
            std::cerr << "\n";
        }
    }

    void load_from_fen(const std::string& fen)
    {
        // TODO: Load eval value

        auto parts = StringUtils::split(fen, ' ');
        assert(parts.size() == 6);
        auto ranks = StringUtils::split(parts[0],'/');
        assert(ranks.size() == 8);

        std::unordered_map<char, ChessPiece> piece_map = {
            {'K', KING}, 
            {'Q', QUEEN}, 
            {'R', ROOK}, 
            {'B', BISHOP}, 
            {'N', KNIGHT}, 
            {'P', PAWN}, 
        };
        
        uint32_t rank_index = 7;
        for (auto& rank : ranks) {
            uint32_t rank_position = 0;
            for (auto& symbol : rank) {
                if (symbol >= '1' && symbol <= '8') {
                    rank_position += (symbol - '0');
                } else {
                    Color color = symbol > 'a' ? Color::BLACK : Color::WHITE;
                    char symbol_normalized = color == Color::BLACK ? symbol - 'a' + 'A' : symbol;
                    ChessPiece piece = piece_map[symbol_normalized];
                    uint64_t mask = 1ULL << (rank_index * 8 + rank_position);
                    this->bitboards[color][piece] |= mask;
                    this->occupancy[color] |= mask;
                    this->total_occupancy |= mask;
                    rank_position++;
                }
            }
            rank_index--;
        }

        // Active color
        Color active_color = parts[1][0] == 'w' ? WHITE : BLACK;

        // Castling rights
        bool can_white_short_castle = parts[2].contains('K');
        bool can_white_long_castle = parts[2].contains('Q');
        bool can_black_short_castle = parts[2].contains('k');
        bool can_black_long_castle = parts[2].contains('q');

        // En passant target square
        bool is_enpassant = parts[3] != "-";
        uint8_t en_passant_position = is_enpassant ? (parts[3][0]-'a') + 8 * (parts[3][1]-'1') : 0;

        // Half moves
        uint8_t half_moves = std::stoi(parts[4]);

        // Full moves
        uint16_t full_moves = std::stoi(parts[5]);

        this->aux = GameStateAux(
            en_passant_position,
            is_enpassant,
            can_white_short_castle,
            can_white_long_castle,
            can_black_short_castle,
            can_black_long_castle,
            half_moves,
            active_color,
            full_moves
        );
    }
};