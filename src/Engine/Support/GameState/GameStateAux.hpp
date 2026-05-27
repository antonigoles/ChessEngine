#pragma once
#include "Engine/Support/Consts.hpp"
#include <cstdint>

class GameStateAux 
{
public:
    uint32_t data;
    // ffffffffffffff eeeeee e cccc hhhhhh t
    // f - 14 bits for full move counter
    // e - 7 bits for en passant - first 6 are for position - last one is for "is active"
    // c - 4 bits for castling
    //      c[0] - castling white short allowed
    //      c[1] - castling white long allowed
    //      c[2] - castling black short allowed
    //      c[3] - castling black long allowed
    // h - 6 bits for half move counter 
    // t - 1 bit for turn, 1 is white turn, 0 is black turn 

    GameStateAux() : data(0) {};

    GameStateAux(
        uint8_t en_passant_position, 
        bool is_en_passant,
        bool white_short_castle,
        bool white_long_castle,
        bool black_short_castle,
        bool black_long_castle,
        uint8_t half_move_counter,
        Color turn,
        uint16_t full_move_counter
    ) {
        data = (uint32_t)turn; // 1 bit long
        data |= (uint32_t)(half_move_counter & 0b111111) << 1; // 6 bits long
        data |= (uint32_t)black_long_castle << 7; // 1 bit long
        data |= (uint32_t)black_short_castle << 8; // 1 bit long
        data |= (uint32_t)white_long_castle << 9; // 1 bit long
        data |= (uint32_t)white_short_castle << 10; // 1 bit long
        data |= (uint32_t)is_en_passant << 11; // 1 bit long
        data |= (uint32_t)(en_passant_position & 0b111111) << 12; // 6 bit long
        data |= (uint32_t)(full_move_counter & 0b11111111111111) << 18; // 14 bit long
    }

    inline Color get_turn() const
    {
        return (Color)(data & 0b1);
    }

    inline uint8_t half_move_count() const
    {
        return (data >> 1) & 0b111111;
    }

    inline bool can_white_short_castle() const
    {
        return (data >> 10) & 0b1; 
    }

    inline bool can_white_long_castle() const
    {
        return (data >> 9) & 0b1; 
    }

    inline bool can_black_short_castle() const
    {
        return (data >> 8) & 0b1; 
    }

    inline bool can_black_long_castle() const
    {
        return (data >> 7) & 0b1; 
    }

    inline bool can_en_passant() const
    {
        return (data >> 11) & 0b1; 
    }

    inline uint8_t en_passant_square()  const
    {
        return (data >> 12) & 0b111111;
    }

    inline uint16_t full_move_count() const
    {
        return (data >> 18) & 0b11111111111111;
    }
};