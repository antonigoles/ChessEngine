#include <Engine/PreparedData/Zobrist.hpp>

void Zobrist::init() 
{
    const int polyglot_piece_map[6] = {2, 4, 6, 8, 0, 10};

    for (int c = 0; c < 2; c++) {
        for (int p = 0; p < 6; p++) {
            int poly_piece = polyglot_piece_map[p] + (c == WHITE ? 1 : 0);
            for (int sq = 0; sq < 64; sq++) {
                piece_keys[c][p][sq] = Polyglot_Random64[64 * poly_piece + sq];
            }
        }
    }

    for (int file = 0; file < 8; file++) {
        en_passant_keys[file] = Polyglot_Random64[772 + file];
    }

    for (int i = 0; i < 16; i++) {
        uint64_t k = 0;
        if (i & 1) k ^= Polyglot_Random64[768];
        if (i & 2) k ^= Polyglot_Random64[769];
        if (i & 4) k ^= Polyglot_Random64[770];
        if (i & 8) k ^= Polyglot_Random64[771];
        castle_keys[i] = k;
    }

    side_to_move_key = Polyglot_Random64[780];
}; 

uint64_t Zobrist::generate_key(GameState& state)
{
    uint64_t key = 0;

    for (int c = 0; c < 2; c++) {
        for (int p = 0; p < 6; p++) {
            uint64_t bb = state.bitboards[c][p];
            while (bb) {
                int sq = std::countr_zero(bb);
                key ^= piece_keys[c][p][sq];
                bb &= bb - 1; 
            }
        }
    }

    if (Zobrist::is_polyglot_ep_active(state)) {
        int ep_sq = state.aux.en_passant_square();
        key ^= en_passant_keys[ep_sq % 8];
    }

    int castle_rights = 0;
    if (state.aux.can_white_short_castle()) castle_rights |= 1;
    if (state.aux.can_white_long_castle())  castle_rights |= 2;
    if (state.aux.can_black_short_castle()) castle_rights |= 4;
    if (state.aux.can_black_long_castle())  castle_rights |= 8;
    
    key ^= castle_keys[castle_rights];

    if (state.aux.get_turn() == WHITE) { 
        key ^= side_to_move_key;
    }

    return key;
};


bool Zobrist::is_polyglot_ep_active(GameState& state) {
    if (!state.aux.can_en_passant()) return false;
    
    int ep_sq = state.aux.en_passant_square();
    Color turn = state.aux.get_turn();
    
    int pawn_sq = (turn == WHITE) ? (ep_sq - 8) : (ep_sq + 8);
    
    if (pawn_sq < 0 || pawn_sq > 63) return false;

    uint64_t attackers_mask = 0;
    int file = pawn_sq % 8;
    if (file > 0) attackers_mask |= (1ULL << (pawn_sq - 1));
    if (file < 7) attackers_mask |= (1ULL << (pawn_sq + 1));
    
    return (state.bitboards[turn][PAWN] & attackers_mask) != 0;
}