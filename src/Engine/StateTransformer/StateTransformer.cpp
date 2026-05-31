#include "Engine/MoveGenerator/MoveGenerator.hpp"
#include "Engine/PreparedData/PreparedData.hpp"
#include "Engine/Support/Consts.hpp"
#include "Engine/Support/GameState/GameStateAux.hpp"
#include "Utils/BitsUtil.hpp"
#include <Engine/StateTransformer/StateTransformer.hpp>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <Engine/PreparedData/Zobrist.hpp>

static inline void recalculate_pawn_structure_score(GameState& state)
{
    state.pawn_structure_score = 0.0f;

    Color us = state.aux.get_turn();
    Color them = (Color)(!us);

    // 1. king safety - basically if there are pawns in front of the king

    // 1a. FIRST WHITE
    uint64_t king_pos = std::countr_zero(state.bitboards[WHITE][KING]);
    uint64_t shield_mask = PreparedData::king_safety_mask[WHITE][king_pos];
    uint64_t active_shield = shield_mask & state.bitboards[WHITE][PAWN];
    int pawns_in_front = std::popcount(active_shield);
    state.pawn_structure_score += ((float)pawns_in_front - 1.5f) * 50.0f;

    // 2a. NOW BLACK
    king_pos = std::countr_zero(state.bitboards[BLACK][KING]);
    shield_mask = PreparedData::king_safety_mask[BLACK][king_pos];
    active_shield = shield_mask & state.bitboards[BLACK][PAWN];
    pawns_in_front = std::popcount(active_shield);
    state.pawn_structure_score += -((float)pawns_in_front - 1.5f) * 50.0f;
    

    // 2. Pawn structure itself

    // 2a. FIRST WHITE
    uint64_t white_pawns = state.bitboards[WHITE][PAWN];
    while (white_pawns) {
        int sq = std::countr_zero(white_pawns);

        if (!(white_pawns & PreparedData::isolated_pawn_mask[sq])) {
            // Fully isolated
            state.pawn_structure_score -= 15.0f;
        } else {
            // is not isolated or semi-isolated?
            uint64_t support_zone = 
                  BitsUtil::shift_cap_west<WHITE>(sq)
                | BitsUtil::shift_cap_east<WHITE>(sq)
                | ((sq & ~BitsUtil::FILE_A) >> 1)
                | ((sq & ~BitsUtil::FILE_H) << 1);

            if (!(white_pawns & support_zone)) {
                // Non-pernament orphan aka semi-isolated
                state.pawn_structure_score -= 8.0f;
            }
        }
    
        
        white_pawns &= white_pawns - 1;
    }

    uint64_t black_pawns = state.bitboards[BLACK][PAWN];
    while (black_pawns) {
        int sq = std::countr_zero(black_pawns);

        if (!(black_pawns & PreparedData::isolated_pawn_mask[sq])) {
            // Fully isolated
            state.pawn_structure_score += 15.0f;
        } else {
            // is not isolated or semi-isolated?
            uint64_t support_zone = 
                  BitsUtil::shift_cap_west<BLACK>(sq)
                | BitsUtil::shift_cap_east<BLACK>(sq)
                | ((sq & ~BitsUtil::FILE_A) >> 1)
                | ((sq & ~BitsUtil::FILE_H) << 1);

            if (!(black_pawns & support_zone)) {
                // Non-pernament orphan aka semi-isolated
                state.pawn_structure_score += 8.0f;
            }
        }
    
        
        black_pawns &= black_pawns - 1;
    }
}

static inline void recalculate_king_safety_score(GameState& state)
{
    state.king_safety_score = 0.0f;

    // 1. We should probably not want to be checked
    state.king_safety_score += (state.is_checked * 200.0f) * (state.aux.get_turn() == WHITE ? -1 : 1);

    // 2. Check attacked fields around king depending on piece
    uint64_t white_king_pos = std::countr_zero(state.bitboards[WHITE][KING]);
    uint64_t black_king_pos = std::countr_zero(state.bitboards[BLACK][KING]);

    // QUEEN
    state.king_safety_score -= 500.0f * std::popcount(state.bitboards[BLACK][QUEEN] & PreparedData::king_safety_ring_mask[white_king_pos]);
    state.king_safety_score += 500.0f * std::popcount(state.bitboards[WHITE][QUEEN] & PreparedData::king_safety_ring_mask[black_king_pos]);

    // ROOK
    state.king_safety_score -= 200.0f * std::popcount(state.bitboards[BLACK][ROOK] & PreparedData::king_safety_ring_mask[white_king_pos]);
    state.king_safety_score += 200.0f * std::popcount(state.bitboards[WHITE][ROOK] & PreparedData::king_safety_ring_mask[black_king_pos]);

    // HORSE
    state.king_safety_score -= 200.0f * std::popcount(state.bitboards[BLACK][KNIGHT] & PreparedData::king_safety_ring_mask[white_king_pos]);
    state.king_safety_score += 200.0f * std::popcount(state.bitboards[WHITE][KNIGHT] & PreparedData::king_safety_ring_mask[black_king_pos]);

    uint64_t white_attackers = 10 * std::popcount(
        (state.occupancy[BLACK] & ~state.bitboards[BLACK][PAWN]) & PreparedData::king_safety_ring_mask[white_king_pos]
    );

    uint64_t black_attackers = 10 * std::popcount(
        (state.occupancy[WHITE] & ~state.bitboards[WHITE][PAWN]) & PreparedData::king_safety_ring_mask[black_king_pos]
    );

    state.king_safety_score -= white_attackers * white_attackers;
    state.king_safety_score += black_attackers * black_attackers;
}

static inline bool is_end_game_test(GameState& state)
{
    uint64_t figures_on_board = std::popcount(state.total_occupancy) - std::popcount(state.bitboards[WHITE][PAWN] | state.bitboards[BLACK][PAWN]);
    return figures_on_board <= 4;
}

bool StateTransformer::apply_move(GameState& state, const ChessMove& move)
{
    if (state.zobrist_key == 0) {
        state.zobrist_key = Zobrist::generate_key(state);
    }

    Color us = state.aux.get_turn();
    Color them = (Color)(!us);

    int old_castle_rights = 0;
    if (state.aux.can_white_short_castle()) old_castle_rights |= 1;
    if (state.aux.can_white_long_castle())  old_castle_rights |= 2;
    if (state.aux.can_black_short_castle()) old_castle_rights |= 4;
    if (state.aux.can_black_long_castle())  old_castle_rights |= 8;

    bool old_has_ep = Zobrist::is_polyglot_ep_active(state);
    int old_ep_file = state.aux.can_en_passant() ? (state.aux.en_passant_square() % 8) : 0;

    bool is_end_game = is_end_game_test(state);

    float new_evaluation_score = state.evaluation_score;

    int64_t from = move.get_from();
    int64_t to = move.get_to();
    uint64_t from_mask = 1ULL << from;
    uint64_t to_mask = 1ULL << to;

    ChessPiece moving_piece = PAWN;
    for (uint8_t piece = 0; piece < 6; piece++) {
        if (state.bitboards[us][piece] & from_mask) {
            moving_piece = static_cast<ChessPiece>(piece);
            break;
        }
    }

    // Add and Remove score for positional change (PSTs)
    new_evaluation_score += us == WHITE ? 
        PST_TABLE[is_end_game][moving_piece][to ^ 56] * PST_MULTIPLIER_TABLE[is_end_game][moving_piece]
        : -PST_TABLE[is_end_game][moving_piece][to] * PST_MULTIPLIER_TABLE[is_end_game][moving_piece];
    
    new_evaluation_score -= us == WHITE ? 
        PST_TABLE[is_end_game][moving_piece][from ^ 56] * PST_MULTIPLIER_TABLE[is_end_game][moving_piece] 
        : -PST_TABLE[is_end_game][moving_piece][from] * PST_MULTIPLIER_TABLE[is_end_game][moving_piece];

    bool is_capture = state.occupancy[them] & to_mask;
    ChessPiece captured_piece = PAWN;
    if (is_capture) {
        for (int p = 0; p < 6; p++) {
            if (state.bitboards[them][p] & to_mask) {
                captured_piece = static_cast<ChessPiece>(p);
                break;
            }
        }

        // Add score for material change
        new_evaluation_score += us == WHITE ? CHESS_PIECE_VALUES[captured_piece] : -CHESS_PIECE_VALUES[captured_piece];

        // Remove score for positional change (PSTs)
        new_evaluation_score -= them == WHITE ? 
            PST_TABLE[is_end_game][captured_piece][to ^ 56] * PST_MULTIPLIER_TABLE[is_end_game][captured_piece]: 
            -PST_TABLE[is_end_game][captured_piece][to] * PST_MULTIPLIER_TABLE[is_end_game][captured_piece];
    }

    // AUX DATA
    uint8_t half_move_count = state.aux.half_move_count();
    bool is_white_short_castle = state.aux.can_white_short_castle();
    bool is_white_long_castle = state.aux.can_white_long_castle();
    bool is_black_short_castle = state.aux.can_black_short_castle();
    bool is_black_long_castle = state.aux.can_black_long_castle();


    if (is_capture || moving_piece == PAWN) {
        half_move_count = 0;
    } else {
        half_move_count++;
    }

    // Moving the piece
    state.bitboards[us][moving_piece] ^= (from_mask | to_mask);

    // If capture - remove the piece from enemys bitboard
    if (is_capture) {
        state.bitboards[them][captured_piece] ^= to_mask;
    }

    int8_t ep_square_this_turn = -1;

    // Special moves
    if (moving_piece == PAWN) {
        // A. PAWN Double move - we need to set en passant
        if (std::abs(to - from) == 16) {
            ep_square_this_turn = (from + to) / 2; // Square behind our pawn, works both ways
        }
        // B. Taking en passant
        else if (!is_capture && to == state.aux.en_passant_square() && state.aux.can_en_passant()) {
            // Not "is capture" but still ended up on en passant square
            int capture_sq = (us == WHITE) ? (to - 8) : (to + 8);
            state.bitboards[them][PAWN] ^= (1ULL << capture_sq);
        }
        // C. Promotion
        else if (move.has_promotion()) {
            // Switch
            state.bitboards[us][PAWN] ^= to_mask; //  Remove the pawn
            state.bitboards[us][move.get_promotion()] ^= to_mask; // Add new piece

            new_evaluation_score -= us == WHITE ? CHESS_PIECE_VALUES[PAWN] : -CHESS_PIECE_VALUES[PAWN];
            new_evaluation_score += us == WHITE ? CHESS_PIECE_VALUES[move.get_promotion()] : -CHESS_PIECE_VALUES[move.get_promotion()];

            new_evaluation_score -= us == WHITE ? PST_TABLE[is_end_game][PAWN][to ^ 56] : -PST_TABLE[is_end_game][PAWN][to];
            new_evaluation_score += us == WHITE ? PST_TABLE[is_end_game][move.get_promotion()][to ^ 56] : -PST_TABLE[is_end_game][move.get_promotion()][to];
        }
    } 
    else if (moving_piece == KING) {
        // D. Castling
        if (std::abs(to - from) == 2) {
            // (almost) Unreadable cluster of operations, let's hope it works
            if (to == 6 || to == 62) { // Short castle
                state.bitboards[us][ROOK] ^= (1ULL << (to + 1)) | (1ULL << (to - 1)); // (H->F)
            } else if (to == 2 || to == 58) { // Long castle
                state.bitboards[us][ROOK] ^= (1ULL << (to - 2)) | (1ULL << (to + 1)); // (A->D)
            }
            new_evaluation_score += (!is_end_game) * (us == WHITE ? CHESS_PIECE_VALUES[PAWN] * 2 : -CHESS_PIECE_VALUES[PAWN] * 2);
        }

        if (us == WHITE) {
            is_white_long_castle = false;
            is_white_short_castle = false;
        } else {
            is_black_long_castle = false;
            is_black_short_castle = false;
        }
    }

    state.occupancy[us] = 
        state.bitboards[us][PAWN] 
        | state.bitboards[us][KNIGHT] 
        | state.bitboards[us][KING] 
        | state.bitboards[us][BISHOP]
        | state.bitboards[us][ROOK]
        | state.bitboards[us][QUEEN];

    state.occupancy[them] = 
        state.bitboards[them][PAWN] 
        | state.bitboards[them][KNIGHT] 
        | state.bitboards[them][KING] 
        | state.bitboards[them][BISHOP]
        | state.bitboards[them][ROOK]
        | state.bitboards[them][QUEEN];

    state.total_occupancy = state.occupancy[us] | state.occupancy[them];

    // Check if rooks have moved

    // 1. Long White Rook
    is_white_long_castle = is_white_long_castle && (state.bitboards[WHITE][ROOK] & 1ULL);
    // 1. Short White Rook
    is_white_short_castle = is_white_short_castle && (state.bitboards[WHITE][ROOK] & (1ULL << 7));
    // 1. Long Black Rook
    is_black_long_castle = is_black_long_castle && (state.bitboards[BLACK][ROOK] & (1ULL << 56));
    // 1. Short Black Rook
    is_black_short_castle = is_black_short_castle && (state.bitboards[BLACK][ROOK] & (1ULL << 63));

    state.aux = GameStateAux(
        static_cast<uint8_t>(ep_square_this_turn == -1 ? 0 : ep_square_this_turn),
        ep_square_this_turn != -1,
        is_white_short_castle,
        is_white_long_castle,
        is_black_short_castle,
        is_black_long_castle,
        half_move_count,
        them,
        state.aux.full_move_count() + (us == BLACK ? 1 : 0)
    );

    state.evaluation_score = new_evaluation_score;

    if (us == WHITE && MoveGenerator::is_our_king_in_check<WHITE>(state)) {
        return false;
    } else if (us == BLACK && MoveGenerator::is_our_king_in_check<BLACK>(state)) {
        return false;
    }

    recalculate_pawn_structure_score(state);
    recalculate_king_safety_score(state);

    if (us == WHITE) {
        state.is_checked = MoveGenerator::is_our_king_in_check<BLACK>(state);
    } else {
        state.is_checked = MoveGenerator::is_our_king_in_check<WHITE>(state);
    }

    // ZOBRIST

    state.zobrist_key ^= Zobrist::side_to_move_key;
    state.zobrist_key ^= Zobrist::piece_keys[us][moving_piece][from];

    if (move.has_promotion()) {
        state.zobrist_key ^= Zobrist::piece_keys[us][move.get_promotion()][to];
    } else {
        state.zobrist_key ^= Zobrist::piece_keys[us][moving_piece][to];
    }

    if (is_capture) {
        state.zobrist_key ^= Zobrist::piece_keys[them][captured_piece][to];
    } 
    else if (moving_piece == PAWN && to == state.aux.en_passant_square() && state.aux.can_en_passant()) {
        int capture_sq = (us == WHITE) ? (to - 8) : (to + 8);
        state.zobrist_key ^= Zobrist::piece_keys[them][PAWN][capture_sq];
    }

    if (moving_piece == KING && std::abs(to - from) == 2) {
        if (to == 6 || to == 62) {
            state.zobrist_key ^= Zobrist::piece_keys[us][ROOK][to + 1];
            state.zobrist_key ^= Zobrist::piece_keys[us][ROOK][to - 1];
        } else if (to == 2 || to == 58) {
            state.zobrist_key ^= Zobrist::piece_keys[us][ROOK][to - 2];
            state.zobrist_key ^= Zobrist::piece_keys[us][ROOK][to + 1]; 
        }
    }

    int new_castle_rights = 0;
    if (state.aux.can_white_short_castle()) new_castle_rights |= 1;
    if (state.aux.can_white_long_castle())  new_castle_rights |= 2;
    if (state.aux.can_black_short_castle()) new_castle_rights |= 4;
    if (state.aux.can_black_long_castle())  new_castle_rights |= 8;

    if (old_castle_rights != new_castle_rights) {
        state.zobrist_key ^= Zobrist::castle_keys[old_castle_rights];
        state.zobrist_key ^= Zobrist::castle_keys[new_castle_rights];
    }

    if (old_has_ep) {
        state.zobrist_key ^= Zobrist::en_passant_keys[old_ep_file];
    }
    
    if (Zobrist::is_polyglot_ep_active(state)) {
        state.zobrist_key ^= Zobrist::en_passant_keys[state.aux.en_passant_square() % 8];
    }
    
    return true;
}
