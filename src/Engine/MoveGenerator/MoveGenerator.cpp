#include "Engine/MoveGenerator/PawnMoves.hpp"
#include "Engine/PreparedData/PreparedData.hpp"
#include "Engine/PreparedData/magic.hpp"
#include "Engine/Support/ChessMove.hpp"
#include "Engine/Support/Consts.hpp"
#include <Engine/MoveGenerator/MoveGenerator.hpp>
#include <bit>

uint64_t MoveGenerator::get_rook_attacks(uint64_t from, uint64_t occupancy)
{
    uint64_t blockers = occupancy & InlinedMagic::rook_magic_table[from].mask;
    uint64_t index = (blockers * InlinedMagic::rook_magic_table[from].magic) >> InlinedMagic::rook_magic_table[from].shift;
    return InlinedMagic::rook_magic_table[from].attacks[index];
}
 
uint64_t MoveGenerator::get_bishop_attacks(uint64_t from, uint64_t occupancy)
{
    uint64_t blockers = occupancy & InlinedMagic::bishop_magic_table[from].mask;
    uint64_t index = (blockers * InlinedMagic::bishop_magic_table[from].magic) >> InlinedMagic::bishop_magic_table[from].shift;
    return InlinedMagic::bishop_magic_table[from].attacks[index];
}

template <Color Them>
bool MoveGenerator::is_square_attacked(int sq, const GameState& state) 
{
    constexpr Color Us = (Color)!Them; 
    if (PreparedData::pawns_attack_table[Us][sq] & state.bitboards[Them][PAWN]) return true;
    if (PreparedData::knight_attack_table[sq] & state.bitboards[Them][KNIGHT]) return true;
    if (PreparedData::king_attack_table[sq] & state.bitboards[Them][KING]) return true;
    uint64_t bishop_rays = get_bishop_attacks(sq, state.total_occupancy);
    if (bishop_rays & (state.bitboards[Them][BISHOP] | state.bitboards[Them][QUEEN])) return true;
    uint64_t rook_rays = get_rook_attacks(sq, state.total_occupancy);
    if (rook_rays & (state.bitboards[Them][ROOK] | state.bitboards[Them][QUEEN])) return true;
    return false;
}

std::vector<ChessMove> MoveGenerator::generate_pseudo_legal_moves(const GameState& state)
{
    if (state.aux.get_turn() == WHITE) {
        return MoveGenerator::aux_generate_pseudo_legal_moves<WHITE>(state);
    } else {
        return MoveGenerator::aux_generate_pseudo_legal_moves<BLACK>(state);
    }
}

template <Color us>
std::vector<ChessMove> MoveGenerator::aux_generate_pseudo_legal_moves(const GameState& state)
{
    std::vector<ChessMove> legal_moves;
    legal_moves.reserve(40);

    constexpr Color them = us == WHITE ? BLACK : WHITE;

    const uint64_t my_pieces = state.occupancy[us];
    const uint64_t enemy_pieces = state.occupancy[them];
    const uint64_t occupancy = state.total_occupancy;

    // Generating moves for Knights
    uint64_t knights = state.bitboards[us][ChessPiece::KNIGHT];
    while (knights) {
        uint64_t from = std::countr_zero(knights);
        uint64_t attacks = PreparedData::knight_attack_table[from] & ~my_pieces;

        while (attacks) {
            uint64_t to = std::countr_zero(attacks);
            ChessMove move(from, to);
            legal_moves.push_back(move);
            attacks &= attacks - 1;
        }

        knights &= knights - 1;
    }

    // Generating moves for Rooks
    uint64_t rooks = state.bitboards[us][ChessPiece::ROOK];
    while (rooks) {
        uint64_t from = std::countr_zero(rooks);
        uint64_t attacks = get_rook_attacks(from, occupancy) & ~my_pieces;

        while (attacks) {
            uint64_t to = std::countr_zero(attacks);
            ChessMove move(from, to);
            legal_moves.push_back(move);
            attacks &= attacks - 1;
        }

        rooks &= rooks - 1;
    }

    // Generating moves for Bishops
    uint64_t bishops = state.bitboards[us][ChessPiece::BISHOP];
    while (bishops) {
        uint64_t from = std::countr_zero(bishops);
        uint64_t attacks = get_bishop_attacks(from, occupancy) & ~my_pieces;

        while (attacks) {
            uint64_t to = std::countr_zero(attacks);
            ChessMove move(from, to);
            legal_moves.push_back(move);
            attacks &= attacks - 1;
        }

        bishops &= bishops - 1;
    }

    // Generating moves for Queens
    uint64_t queens = state.bitboards[us][ChessPiece::QUEEN];
    while (queens) {
        uint64_t from = std::countr_zero(queens);
        uint64_t attacks = get_bishop_attacks(from, occupancy) & ~my_pieces;
        attacks |= get_rook_attacks(from, occupancy) & ~my_pieces;

        while (attacks) {
            uint64_t to = std::countr_zero(attacks);
            ChessMove move(from, to);
            legal_moves.push_back(move);
            attacks &= attacks - 1;
        }

        queens &= queens - 1;
    }

    // Generating moves for King
    uint64_t kings = state.bitboards[us][ChessPiece::KING];
    while (kings) {
        uint64_t from = std::countr_zero(kings);
        uint64_t attacks = PreparedData::king_attack_table[from] & ~my_pieces;

        while (attacks) {
            uint64_t to = std::countr_zero(attacks);
            ChessMove move(from, to);
            legal_moves.push_back(move);
            attacks &= attacks - 1;
        }

        kings &= kings - 1;
    }

    generate_pawn_moves<us>(state, legal_moves);

    uint64_t occ = state.total_occupancy;

    constexpr uint64_t W_SHORT_EMPTY = (1ULL << 5) | (1ULL << 6);               // F1, G1
    constexpr uint64_t W_LONG_EMPTY  = (1ULL << 1) | (1ULL << 2) | (1ULL << 3); // B1, C1, D1
    constexpr uint64_t B_SHORT_EMPTY = (1ULL << 61) | (1ULL << 62);             // F8, G8
    constexpr uint64_t B_LONG_EMPTY  = (1ULL << 57) | (1ULL << 58) | (1ULL << 59); // B8, C8, D8

    // Handle Castling
    if constexpr (us == WHITE) {
        // Short castle (O-O)
        if (state.aux.can_white_short_castle() && !(occ & W_SHORT_EMPTY)) {
            if (!is_square_attacked<them>(4, state) && 
                !is_square_attacked<them>(5, state) && 
                !is_square_attacked<them>(6, state)) {
                legal_moves.emplace_back(4, 6);
            }
        }

        // Long castle (O-O-O)
        if (state.aux.can_white_long_castle() && !(occ & W_LONG_EMPTY)) {
            if (!is_square_attacked<them>(4, state) && 
                !is_square_attacked<them>(3, state) && 
                !is_square_attacked<them>(2, state)) {
                legal_moves.emplace_back(4, 2); 
            }
        }

    } else { // BLACK
        
        // Short castle (O-O)
        if (state.aux.can_black_short_castle() && !(occ & B_SHORT_EMPTY)) {
            if (!is_square_attacked<them>(60, state) && 
                !is_square_attacked<them>(61, state) && 
                !is_square_attacked<them>(62, state)) {
                legal_moves.emplace_back(60, 62);
            }
        }

        // Long castle (O-O-O)
        if (state.aux.can_black_long_castle() && !(occ & B_LONG_EMPTY)) {
            if (!is_square_attacked<them>(60, state) && 
                !is_square_attacked<them>(59, state) && 
                !is_square_attacked<them>(58, state)) {
                legal_moves.emplace_back(60, 58); 
            }
        }
    }
    
    return legal_moves;
};
