#pragma once
#include <cstdint>
#include <string>
#include <assert.h>
#include <Engine/Support/Consts.hpp>

class ChessMove {
public:
    uint16_t data;
    // Format explained
    // e z pp ffffff tttttt
    // e - 1 bit for "en passant"
    // z - 1 bit for "has promotion"
    // p - 2 bits for promotion
    // f - 6 bits for "from" position
    // t - 6 bits for "to" position


    ChessMove() : data(0) {}

    ChessMove(uint8_t from, uint8_t to, ChessPiece promotion = ChessPiece::KNIGHT, bool has_promotion = false) {
        data = 
            (to & 0b111111) 
            | (((uint16_t)from & 0b111111) << 6) 
            | ((uint16_t)(promotion & 0b11) << 12)
            | ((uint16_t)has_promotion << 14);
    }

    ChessMove(uint8_t from, uint8_t to, bool en_passant) {
        data = 
            (to & 0b111111) 
            | (((uint16_t)from & 0b111111) << 6) 
            | ((uint16_t)en_passant << 15);
    }

    bool is_en_passant() const
    {
        return (data >> 15) & 0b1;
    }

    uint8_t get_from() const
    {
        return (data >> 6) & (0b111111);
    }

    uint8_t get_to() const
    {
        return (data & 0b111111);
    }

    bool has_promotion() const
    {
        return (data >> 14) & 0b1;
    }

    ChessPiece get_promotion() const
    {
        return (ChessPiece)((data >> 12) & 0b11);
    }

    std::string to_string() const
    {
        std::string result = "";
        uint8_t from = this->get_from();
        uint8_t to = this->get_to();

        result += 'a' + (from % 8);
        result += '1' + (from / 8);

        result += 'a' + (to % 8);
        result += '1' + (to / 8);

        if (this->has_promotion()) {
            ChessPiece piece = this->get_promotion();
            switch (piece)
            {
                case ChessPiece::KNIGHT:
                    result += 'n';
                    break;
                case ChessPiece::ROOK:
                    result += 'r';
                    break;
                case ChessPiece::QUEEN:
                    result += 'q';
                    break;
                case ChessPiece::BISHOP:
                    result += 'b';
                    break;
                default:
                    break;
            }
        }
        return result;
    }

    static ChessMove from_string(const std::string& str)
    {
        assert(str.size() >= 4 && str.size() <= 5);
        ChessMove move;
        move.data |= ((str[0] - 'a') + (str[1] - '1') * 8) << 6; // FROM move
        move.data |= ((str[2] - 'a') + (str[3] - '1') * 8); // TO move
        if (str.size() == 5) {
            move.data |= 0b1 << 14;
            switch (str[4])
            {
                case 'n':
                    move.data |= (uint16_t)ChessPiece::KNIGHT << 12;
                    break;
                case 'r':
                    move.data |= (uint16_t)ChessPiece::ROOK << 12;
                    break;
                case 'q':
                    move.data |= (uint16_t)ChessPiece::QUEEN << 12;
                    break;
                case 'b':
                    move.data |= (uint16_t)ChessPiece::BISHOP << 12;
                    break;
            }
        }
        return move;
    }
};