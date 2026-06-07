#include "Engine/MoveGenerator/MoveGenerator.hpp"
#include "Engine/PreparedData/PreparedData.hpp"
#include "Engine/Support/Consts.hpp"
#include "Engine/Support/GameState/GameState.hpp"
#include "Engine/Support/GameState/GameStateAux.hpp"
#include "Utils/BitsUtil.hpp"
#include <Engine/StateTransformer/StateTransformer.hpp>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <Engine/PreparedData/Zobrist.hpp>

static inline uint64_t get_pawn_attacks_mask(Color color, uint64_t pawns)
{
    return color == WHITE ? 
        BitsUtil::shift_cap_west<WHITE>(pawns) | BitsUtil::shift_cap_east<WHITE>(pawns) :
        BitsUtil::shift_cap_west<BLACK>(pawns) | BitsUtil::shift_cap_east<BLACK>(pawns);
}

static inline void recalculate_pawn_structure_score(GameState& state)
{
    state.pawn_structure_score.isolated_pawns = 0.0f;
    state.pawn_structure_score.semi_isolated_pawns = 0.0f;
    state.pawn_structure_score.passed_pawns = 0.0f;
    state.pawn_structure_score.doubled_pawns = 0.0f;
    
    uint64_t white_pawns = state.bitboards[WHITE][PAWN];
    uint64_t black_pawns = state.bitboards[BLACK][PAWN];

    // Doubled pawns
    uint64_t w_files = white_pawns;
    w_files |= w_files >> 32;
    w_files |= w_files >> 16; 
    w_files |= w_files >> 8;

    int w_unique_files = std::popcount(w_files & 0xFF);
    int w_extra_pawns = std::popcount(white_pawns) - w_unique_files;
    state.pawn_structure_score.doubled_pawns -= w_extra_pawns * 15.0f;

    uint64_t b_files = black_pawns;
    b_files |= b_files >> 32;
    b_files |= b_files >> 16;
    b_files |= b_files >> 8;

    int b_unique_files = std::popcount(b_files & 0xFF);
    int b_extra_pawns = std::popcount(black_pawns) - b_unique_files;
    state.pawn_structure_score.doubled_pawns += b_extra_pawns * 15.0f;

    // 2a. FIRST WHITE
    while (white_pawns) {
        int sq = std::countr_zero(white_pawns);

        if (!(white_pawns & PreparedData::isolated_pawn_mask[sq])) {
            // Fully isolated
            state.pawn_structure_score.isolated_pawns -= 15.0f;
        } else {
            // is not isolated or semi-isolated?
            uint64_t support_zone = 
                  BitsUtil::shift_cap_west<WHITE>(sq)
                | BitsUtil::shift_cap_east<WHITE>(sq)
                | ((sq & ~BitsUtil::FILE_A) >> 1)
                | ((sq & ~BitsUtil::FILE_H) << 1);

            if (!(white_pawns & support_zone)) {
                // Non-pernament orphan aka semi-isolated
                state.pawn_structure_score.semi_isolated_pawns -= 8.0f;
            }
        }
    
        // Passed pawns
        if ((PreparedData::passed_pawn_masks[WHITE][sq] & state.bitboards[BLACK][PAWN]) == 0) {
            int rank = sq / 8;
            state.pawn_structure_score.passed_pawns += PASSED_PAWN_BONUS[rank];
        }
        
        white_pawns &= white_pawns - 1;
    }

    // 2b. THEN BLACK
    while (black_pawns) {
        int sq = std::countr_zero(black_pawns);

        if (!(black_pawns & PreparedData::isolated_pawn_mask[sq])) {
            // Fully isolated
            state.pawn_structure_score.isolated_pawns += 15.0f;
        } else {
            // is not isolated or semi-isolated?
            uint64_t support_zone = 
                  BitsUtil::shift_cap_west<BLACK>(sq)
                | BitsUtil::shift_cap_east<BLACK>(sq)
                | ((sq & ~BitsUtil::FILE_A) >> 1)
                | ((sq & ~BitsUtil::FILE_H) << 1);

            if (!(black_pawns & support_zone)) {
                // Non-pernament orphan aka semi-isolated
                state.pawn_structure_score.semi_isolated_pawns += 8.0f;
            }
        }

        // Passed pawns
        if ((PreparedData::passed_pawn_masks[BLACK][sq] & state.bitboards[WHITE][PAWN]) == 0) {
            int rank = 7 - (sq / 8); 
            state.pawn_structure_score.passed_pawns -= PASSED_PAWN_BONUS[rank];
        }
    
        black_pawns &= black_pawns - 1;
    }
}

static inline void recalculate_king_safety_score(GameState& state)
{
    state.king_safety_score.attackers = 0.0f;
    state.king_safety_score.pawns_in_front = 0.0f;


    // 1. Check attacked fields around king depending on piece
    uint64_t white_king_pos = std::countr_zero(state.bitboards[WHITE][KING]);
    uint64_t black_king_pos = std::countr_zero(state.bitboards[BLACK][KING]);

    Color us = state.aux.get_turn();
    Color them = (Color)(!us);

    // Are there are pawns in front of the king

    // 1a. FIRST WHITE
    uint64_t king_pos = std::countr_zero(state.bitboards[WHITE][KING]);
    uint64_t shield_mask = PreparedData::king_safety_mask[WHITE][king_pos];
    uint64_t active_shield = shield_mask & state.bitboards[WHITE][PAWN];
    int pawns_in_front = std::popcount(active_shield);
    state.king_safety_score.pawns_in_front += ((float)pawns_in_front - 1.5f) * 50.0f;

    // 2a. NOW BLACK
    king_pos = std::countr_zero(state.bitboards[BLACK][KING]);
    shield_mask = PreparedData::king_safety_mask[BLACK][king_pos];
    active_shield = shield_mask & state.bitboards[BLACK][PAWN];
    pawns_in_front = std::popcount(active_shield);
    state.king_safety_score.pawns_in_front += -((float)pawns_in_front - 1.5f) * 50.0f;


    uint64_t occ_all = state.total_occupancy;
    int64_t white_attackers_count = 0;
    int64_t white_attack_weight = 0;

    int64_t black_attackers_count = 0;
    int64_t black_attack_weight = 0;

    for (int p = KNIGHT; p <= QUEEN; ++p) {
        uint64_t bb = state.bitboards[BLACK][p];
        while (bb) {
            int sq = std::countr_zero(bb);
            bb &= (bb - 1);

            uint64_t attacks = 0;
            switch(p) {
                case KNIGHT: attacks = PreparedData::knight_attack_table[sq]; break;
                case BISHOP: attacks = MoveGenerator::get_bishop_attacks(sq, occ_all); break;
                case ROOK:   attacks = MoveGenerator::get_rook_attacks(sq, occ_all); break;
                case QUEEN:  
                    attacks = MoveGenerator::get_bishop_attacks(sq, occ_all) 
                    | MoveGenerator::get_rook_attacks(sq, occ_all); 
                    break;
            }

            if (attacks & PreparedData::king_safety_ring_mask_small[white_king_pos]) {
                white_attackers_count++;
                white_attack_weight += ATTACK_WEIGHT[p];
            }
        }

        bb = state.bitboards[WHITE][p];
        while (bb) {
            int sq = std::countr_zero(bb);
            bb &= (bb - 1);

            uint64_t attacks = 0;
            switch(p) {
                case KNIGHT: attacks = PreparedData::knight_attack_table[sq]; break;
                case BISHOP: attacks = MoveGenerator::get_bishop_attacks(sq, occ_all); break;
                case ROOK:   attacks = MoveGenerator::get_rook_attacks(sq, occ_all); break;
                case QUEEN:  
                    attacks = MoveGenerator::get_bishop_attacks(sq, occ_all) 
                    | MoveGenerator::get_rook_attacks(sq, occ_all); 
                    break;
            }

            if (attacks & PreparedData::king_safety_ring_mask_small[black_king_pos]) {
                black_attackers_count++;
                black_attack_weight += ATTACK_WEIGHT[p];
            }
        }
    }

    state.king_safety_score.attackers -= white_attackers_count < 2 ? 0.0f : SAFETY_TABLE[white_attack_weight > 19 ? 19 : white_attack_weight]; 
    state.king_safety_score.attackers += black_attackers_count < 2 ? 0.0f : SAFETY_TABLE[black_attack_weight > 19 ? 19 : black_attack_weight]; 
}

static inline void recalculate_safe_mobility_score(GameState& state)
{
    state.safe_mobility_score = 0.0f;
    uint64_t occ_all = state.occupancy[WHITE] | state.occupancy[BLACK];

    uint64_t white_pawn_attacks = get_pawn_attacks_mask(WHITE, state.bitboards[WHITE][PAWN]);
    uint64_t black_pawn_attacks = get_pawn_attacks_mask(BLACK, state.bitboards[BLACK][PAWN]);

    uint64_t white_safe_squares = ~state.occupancy[WHITE] & ~black_pawn_attacks;

    for (int p = KNIGHT; p <= QUEEN; ++p) {
        uint64_t bb = state.bitboards[WHITE][p];
        while (bb) {
            int sq = std::countr_zero(bb);
            bb &= (bb - 1);

            uint64_t attacks = 0;
            switch(p) {
                case KNIGHT: attacks = PreparedData::knight_attack_table[sq]; break;
                case BISHOP: attacks = MoveGenerator::get_bishop_attacks(sq, occ_all); break;
                case ROOK:   attacks = MoveGenerator::get_rook_attacks(sq, occ_all); break;
                case QUEEN:  attacks = MoveGenerator::get_bishop_attacks(sq, occ_all) | MoveGenerator::get_rook_attacks(sq, occ_all); break;
            }

            uint64_t valid_moves = attacks & white_safe_squares;
            int mobility = std::popcount(valid_moves);

            state.safe_mobility_score += mobility * MOBILITY_BONUS[p];
        }
    }

    uint64_t black_safe_squares = ~state.occupancy[BLACK] & ~white_pawn_attacks;

    for (int p = KNIGHT; p <= QUEEN; ++p) {
        uint64_t bb = state.bitboards[BLACK][p];
        while (bb) {
            int sq = std::countr_zero(bb);
            bb &= (bb - 1);

            uint64_t attacks = 0;
            switch(p) {
                case KNIGHT: attacks = PreparedData::knight_attack_table[sq]; break;
                case BISHOP: attacks = MoveGenerator::get_bishop_attacks(sq, occ_all); break;
                case ROOK:   attacks = MoveGenerator::get_rook_attacks(sq, occ_all); break;
                case QUEEN:  attacks = MoveGenerator::get_bishop_attacks(sq, occ_all) | MoveGenerator::get_rook_attacks(sq, occ_all); break;
            }

            uint64_t valid_moves = attacks & black_safe_squares;
            int mobility = std::popcount(valid_moves);

            state.safe_mobility_score -= mobility * MOBILITY_BONUS[p];
        }
    }
}

static inline void recalculate_development_score(GameState& state)
{
    state.early_game_development_score = 0.0f;
    state.mid_game_development_score = 0.0f;


    // 1. Sleeping pieces

    uint64_t undeveloped_white_pieces = (state.bitboards[WHITE][BISHOP] & 0x0000000000000024ULL)
                                        | (state.bitboards[WHITE][KNIGHT] & 0x0000000000000042ULL)
                                        | (state.bitboards[WHITE][KNIGHT] & 0x0000000000000042ULL);

    uint64_t undeveloped_black_pieces = (state.bitboards[BLACK][BISHOP] & 0x2400000000000000ULL)
                                        | (state.bitboards[BLACK][KNIGHT] & 0x4200000000000000ULL)
                                        | (state.bitboards[BLACK][KNIGHT] & 0x4200000000000000ULL);

    state.mid_game_development_score -= 
        (std::popcount(undeveloped_white_pieces) 
        - std::popcount(undeveloped_black_pieces)) * 100.0f;


    // 2. Early Queen move
    state.early_game_development_score -= 
        (std::popcount(state.bitboards[WHITE][QUEEN] & 0x00ffffffffffff00)
        - std::popcount(state.bitboards[BLACK][QUEEN] & 0x00ffffffffffff00)) * QUEEN_EARLY_DEVELOPMENT_PENALTY;
}

static inline void recalculate_piece_activity(GameState& state)
{
    state.active_piece_score = 0.0f;

    uint64_t black_pawn_attacks = get_pawn_attacks_mask(BLACK, state.bitboards[BLACK][PAWN]);
    uint64_t white_pawn_attacks = get_pawn_attacks_mask(WHITE, state.bitboards[WHITE][PAWN]);
    
    // 1. Piece safety / threats
    uint64_t threat_penalty_white = 
            std::popcount(state.bitboards[WHITE][KNIGHT] & black_pawn_attacks) * 0.40f
        +   std::popcount(state.bitboards[WHITE][BISHOP] & black_pawn_attacks) * 0.40f
        +   std::popcount(state.bitboards[WHITE][ROOK] & black_pawn_attacks) * 0.60f
        +   std::popcount(state.bitboards[WHITE][QUEEN] & black_pawn_attacks) * 0.80f;

    uint64_t threat_penalty_black = 
            std::popcount(state.bitboards[BLACK][KNIGHT] & white_pawn_attacks) * 0.40f
        +   std::popcount(state.bitboards[BLACK][BISHOP] & white_pawn_attacks) * 0.40f
        +   std::popcount(state.bitboards[BLACK][ROOK] & white_pawn_attacks) * 0.60f
        +   std::popcount(state.bitboards[BLACK][QUEEN] & white_pawn_attacks) * 0.80f;

    state.active_piece_score -= (threat_penalty_white - threat_penalty_black) * 100.0f;

    // 2. Bishop pair
    state.active_piece_score += std::popcount(state.bitboards[WHITE][BISHOP]) > 2 ? 50.0f : 0.0f;
    state.active_piece_score -= std::popcount(state.bitboards[BLACK][BISHOP]) > 2 ? 50.0f : 0.0f;

    // 3. 7th and 2nd rank rooks
    state.active_piece_score += 
        (std::popcount(state.bitboards[WHITE][ROOK] & 0x00FF000000000000ULL) 
        - std::popcount(state.bitboards[BLACK][ROOK] & 0x000000000000FF00ULL)) * 40.0f;
}

static inline void recalculate_space_advantage(GameState& state)
{
    state.space_advantage_score = 0.0f;

    constexpr uint64_t RANK_3 = 0x0000000000FF0000ULL;
    constexpr uint64_t RANK_4 = 0x00000000FF000000ULL;
    constexpr uint64_t RANK_5 = 0x000000FF00000000ULL;
    constexpr uint64_t RANK_6 = 0x0000FF0000000000ULL;

    constexpr uint64_t FILE_A = 0x0101010101010101ULL;

    uint64_t white_pawns = state.bitboards[WHITE][PAWN];
    uint64_t black_pawns = state.bitboards[BLACK][PAWN];

    // WHITE
    uint64_t w_rank4 = white_pawns & RANK_4;
    uint64_t w_rank5 = white_pawns & RANK_5;
    uint64_t w_rank6 = white_pawns & RANK_6;

    state.space_advantage_score += std::popcount(w_rank4) * 50.0f;
    state.space_advantage_score += std::popcount(w_rank5) * 100.0f;
    state.space_advantage_score += std::popcount(w_rank6) * 200.0f;


    uint64_t w_phalanx = white_pawns & ((white_pawns & ~FILE_A) >> 1);
    state.space_advantage_score += std::popcount(w_phalanx) * 50.0f;

    // BLACK
    uint64_t b_rank4 = black_pawns & RANK_5; 
    uint64_t b_rank5 = black_pawns & RANK_4; 
    uint64_t b_rank6 = black_pawns & RANK_3; 

    state.space_advantage_score -= std::popcount(b_rank4) * 50.0f;
    state.space_advantage_score -= std::popcount(b_rank5) * 100.0f;
    state.space_advantage_score -= std::popcount(b_rank6) * 200.0f;

    // Falanga Czarnych
    uint64_t b_phalanx = black_pawns & ((black_pawns & ~FILE_A) >> 1);
    state.space_advantage_score -= std::popcount(b_phalanx) * 50.0f;
}

void apply_pst_move(GameState& state, ChessPiece piece, int64_t from, int64_t to)
{
    Color us = state.aux.get_turn();

    state.mid_game_score[us] -= PreparedData::mg_pst_table[us][piece][from];
    state.end_game_score[us] -= PreparedData::eg_pst_table[us][piece][from];

    state.mid_game_score[us] += PreparedData::mg_pst_table[us][piece][to];
    state.end_game_score[us] += PreparedData::eg_pst_table[us][piece][to];
}

void apply_pst_capture(GameState& state, ChessPiece piece, int64_t from)
{
    Color them = static_cast<Color>(!state.aux.get_turn());

    state.mid_game_score[them] -= PreparedData::mg_pst_table[them][piece][from];
    state.end_game_score[them] -= PreparedData::eg_pst_table[them][piece][from];

    state.game_phase -= gamephase_inc[piece];
}

void apply_pst_promotion(GameState& state, ChessPiece piece_from, ChessPiece piece_to, int64_t square)
{
    Color us = state.aux.get_turn();

    state.mid_game_score[us] -= PreparedData::mg_pst_table[us][piece_from][square];
    state.end_game_score[us] -= PreparedData::eg_pst_table[us][piece_from][square];

    state.mid_game_score[us] += PreparedData::mg_pst_table[us][piece_to][square];
    state.end_game_score[us] += PreparedData::eg_pst_table[us][piece_to][square];

    state.game_phase -= gamephase_inc[piece_from]; 
    state.game_phase += gamephase_inc[piece_to];
}

void StateTransformer::hard_eval_refresh(GameState& state)
{
    recalculate_pawn_structure_score(state);
    recalculate_king_safety_score(state);
    recalculate_safe_mobility_score(state);
    recalculate_development_score(state);
    recalculate_piece_activity(state);
    recalculate_space_advantage(state);

    // calculate material balance and gamephase
    state.mid_game_score[0] = 0.0f;
    state.mid_game_score[1] = 0.0f;
    state.end_game_score[0] = 0.0f;
    state.end_game_score[1] = 0.0f;
    state.game_phase = 0;

    for (int color = 0; color < 2; ++color) 
    {
        for (int piece = 0; piece < 6; ++piece) 
        {
            uint64_t bitboard = state.bitboards[color][piece];
            
            while (bitboard) 
            {
                int sq;
                sq = std::countr_zero(bitboard);
                state.mid_game_score[color] += PreparedData::mg_pst_table[color][piece][sq];
                state.end_game_score[color] += PreparedData::eg_pst_table[color][piece][sq];
                
                state.game_phase += gamephase_inc[piece];
                
                bitboard &= bitboard - 1; 
            }
        }
    }
    const int MAX_PHASE = 24; 
    int current_phase = (state.game_phase > MAX_PHASE) ? MAX_PHASE : state.game_phase;
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
    apply_pst_move(state, moving_piece, from, to);

    bool is_capture = state.occupancy[them] & to_mask;
    ChessPiece captured_piece = PAWN;
    if (is_capture) {
        for (int p = 0; p < 6; p++) {
            if (state.bitboards[them][p] & to_mask) {
                captured_piece = static_cast<ChessPiece>(p);
                break;
            }
        }

        // Remove score for positional change (PSTs)
        apply_pst_capture(state, captured_piece, to);
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

            apply_pst_capture(state, PAWN, capture_sq);
        }
        // C. Promotion
        else if (move.has_promotion()) {
            // Switch
            state.bitboards[us][PAWN] ^= to_mask; //  Remove the pawn
            state.bitboards[us][move.get_promotion()] ^= to_mask; // Add new piece

            apply_pst_promotion(state, PAWN, move.get_promotion(), to);
        }
    } 
    else if (moving_piece == KING) {
        // D. Castling
        if (std::abs(to - from) == 2) {
            // (almost) Unreadable cluster of operations, let's hope it works
            if (to == 6 || to == 62) { // Short castle
                state.bitboards[us][ROOK] ^= (1ULL << (to + 1)) | (1ULL << (to - 1)); // (H->F)
                apply_pst_move(state, ROOK, to + 1, to - 1);
            } else if (to == 2 || to == 58) { // Long castle
                state.bitboards[us][ROOK] ^= (1ULL << (to - 2)) | (1ULL << (to + 1)); // (A->D)
                apply_pst_move(state, ROOK, to - 2, to + 1);
            }

            // Comment this out: PST tables should reward us for castling
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

    if (us == WHITE && MoveGenerator::is_our_king_in_check<WHITE>(state)) {
        return false;
    } else if (us == BLACK && MoveGenerator::is_our_king_in_check<BLACK>(state)) {
        return false;
    }

    recalculate_pawn_structure_score(state);
    recalculate_king_safety_score(state);
    recalculate_safe_mobility_score(state);
    recalculate_development_score(state);
    recalculate_piece_activity(state);
    recalculate_space_advantage(state);

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

bool StateTransformer::is_king_attacked_after_move(const GameState& state, const ChessMove& move)
{
    Color us = state.aux.get_turn();
    Color them = static_cast<Color>(!us);
    
    int from = move.get_from();
    int to = move.get_to();
    int enemy_king_sq = std::countr_zero(state.bitboards[them][KING]);

    ChessPiece piece = PAWN;
    for (int p = 0; p < 6; p++) {
        if (state.bitboards[us][p] & (1ULL << from)) {
            piece = static_cast<ChessPiece>(p);
            break;
        }
    }

    ChessPiece piece_on_to = move.has_promotion() ? move.get_promotion() : piece;

    uint64_t occ_after = (state.occupancy[WHITE] | state.occupancy[BLACK]);
    occ_after = (occ_after ^ (1ULL << from)) | (1ULL << to);

    bool is_en_passant = (piece == PAWN && !move.has_promotion() && move.get_promotion() == KNIGHT && to == state.aux.en_passant_square());
    if (is_en_passant) {
        int captured_pawn_sq = (us == WHITE) ? to - 8 : to + 8;
        occ_after ^= (1ULL << captured_pawn_sq);
    }

    bool direct_check = false;
    switch (piece_on_to) {
        case PAWN: {
            if (PreparedData::pawns_attack_table[us][to] & (1ULL << enemy_king_sq)) direct_check = true;
            break;
        }
        case KNIGHT:
            if (PreparedData::knight_attack_table[to] & (1ULL << enemy_king_sq)) direct_check = true;
            break;
        case BISHOP:
            if (MoveGenerator::get_bishop_attacks(to, occ_after) & (1ULL << enemy_king_sq)) direct_check = true;
            break;
        case ROOK:
            if (MoveGenerator::get_rook_attacks(to, occ_after) & (1ULL << enemy_king_sq)) direct_check = true;
            break;
        case QUEEN:
            if (MoveGenerator::get_bishop_attacks(to, occ_after) & (1ULL << enemy_king_sq)) direct_check = true;
            if (MoveGenerator::get_rook_attacks(to, occ_after) & (1ULL << enemy_king_sq)) direct_check = true;
            break;
        default: break; 
    }

    if (direct_check) return true;

    uint64_t our_diagonals = (state.bitboards[us][BISHOP] | state.bitboards[us][QUEEN]) & ~(1ULL << from);
    if (our_diagonals) {
        if (MoveGenerator::get_bishop_attacks(enemy_king_sq, occ_after) & our_diagonals) return true;
    }

    uint64_t our_straights = (state.bitboards[us][ROOK] | state.bitboards[us][QUEEN]) & ~(1ULL << from);
    if (our_straights) {
        if (MoveGenerator::get_rook_attacks(enemy_king_sq, occ_after) & our_straights) return true;
    }

    return false;
}