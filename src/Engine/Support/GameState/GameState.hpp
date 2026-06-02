#pragma once
#include "Engine/Support/Consts.hpp"
#include "Utils/StringUtils.hpp"
#include <cassert>
#include <string>
#include <Engine/Support/GameState/GameStateAux.hpp>
#include <unordered_map>
#include <iostream>

class GameState {
public:
    float pawn_structure_score = 0.0f;

    float midgame_king_safety_score = 0.0f;
    float allgame_king_safety_score = 0.0f;
    float king_safety_score = 0.0f;

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

    void recalc_phase_scores()
    {
        int64_t mg_score_delta = this->mid_game_score[WHITE] - this->mid_game_score[BLACK]; 
        int64_t eg_score_delta = this->end_game_score[WHITE] - this->end_game_score[BLACK]; 

        int64_t mg_phase = this->game_phase > 24 ? 24 : this->game_phase; 
        int64_t eg_phase = 24 - mg_phase;

        int64_t final_pst_score = (mg_score_delta * mg_phase + eg_score_delta * eg_phase) / 24;
        this->pst_score = static_cast<float>(final_pst_score); 


        // MID GAME only heuristics
        float mid_game_phase_weight = static_cast<float>(this->game_phase - 12) / 12.0f;
        
        this->king_safety_score = 
            (this->game_phase >= 12 ? midgame_king_safety_score * mid_game_phase_weight : 0.0f)
            + this->allgame_king_safety_score;

        this->development_score = this->game_phase >= 12 
            ? mid_game_development_score * mid_game_phase_weight : 0.0f;


        // EARLY GAME ony heuristics
        float early_game_phase_weight = static_cast<float>(this->game_phase - 20) / 4.0f;
        this->development_score += this->game_phase >= 20 
            ? this->early_game_development_score * early_game_phase_weight : 0.0f;
    }

    float eval_position() const
    {
        return 
            pawn_structure_score * PAWN_STRUCTURE_SCORE_MULTIPLIER
            + king_safety_score * KING_SAFETY_SCORE_MULTIPLIER 
            + pst_score * PST_SCORE_MULTIPLIER
            + safe_mobility_score * MOBILITY_SCORE_MULTIPLIER
            + development_score * DEVELOPMENT_SCORE_MULTIPLIER
            + active_piece_score * ACTIVE_PIECE_MULTIPLIER
            + space_advantage_score * SPACE_ADVANTAGE_MULTIPLIER;
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