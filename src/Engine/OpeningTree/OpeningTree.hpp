#pragma once
#include "Engine/Support/ChessMove.hpp"
#include <unordered_map>
#include <optional>
#include <fstream>
#include <iostream>

#if defined(_MSC_VER)
    #include <stdlib.h>
    #define BSWAP_64 _byteswap_uint64
    #define BSWAP_16 _byteswap_ushort
#elif defined(__APPLE__)
    #include <libkern/OSByteOrder.h>
    #define BSWAP_64 OSSwapInt64
    #define BSWAP_16 OSSwapInt16
#else
    #include <byteswap.h>
    #define BSWAP_64 bswap_64
    #define BSWAP_16 bswap_16
#endif

struct PolyglotEntry {
    uint64_t key;
    uint16_t move;
    uint16_t weight;
    uint32_t learn;
};

class OpeningTree
{
private:
    struct BestMoveInfo {
        uint16_t move;
        uint16_t weight;
    };

    std::unordered_map<uint64_t, BestMoveInfo> tree;


public:
    OpeningTree(const std::string& path) 
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Blad: Nie udalo sie otworzyc ksiegi otwarc: " << path << "\n";
            return;
        }

        PolyglotEntry entry;
        while (file.read(reinterpret_cast<char*>(&entry), sizeof(PolyglotEntry))) {
            // Konwersja Big-Endian -> Little-Endian
            uint64_t key = BSWAP_64(entry.key);
            uint16_t move = BSWAP_16(entry.move);
            uint16_t weight = BSWAP_16(entry.weight);
            auto it = tree.find(key);
            if (it == tree.end() || weight > it->second.weight) {
                tree[key] = {move, weight};
            }
        }
    }

    std::optional<ChessMove> get_next(uint64_t key)
    {
        auto it = tree.find(key);
        if (it == tree.end()) {
            return std::nullopt;
        }

        uint16_t poly_move = it->second.move;

        // Bity 0-5: to_square
        // Bity 6-11: from_square
        // Bity 12-14: promotion_piece (1=Skoczek, 2=Goniec, 3=Wieża, 4=Hetman, 0=Brak)
        
        uint8_t to = poly_move & 0x3F;
        uint8_t from = (poly_move >> 6) & 0x3F;
        uint8_t promo = (poly_move >> 12) & 0x07;

        // PolyGlot castling
        if (from == 4 && to == 7) to = 6;
        else if (from == 4 && to == 0) to = 2;
        else if (from == 60 && to == 63) to = 62;
        else if (from == 60 && to == 56) to = 58;

        if (promo != 0) {
            ChessPiece piece = ChessPiece::QUEEN;
            if (promo == 1) piece = ChessPiece::KNIGHT;
            else if (promo == 2) piece = ChessPiece::BISHOP;
            else if (promo == 3) piece = ChessPiece::ROOK;
            else if (promo == 4) piece = ChessPiece::QUEEN;

            return ChessMove(from, to, piece, true);
        }

        return ChessMove(from, to, ChessPiece::KNIGHT, false);
    }
};