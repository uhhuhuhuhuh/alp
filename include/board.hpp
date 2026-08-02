#pragma once

#include <array>

namespace chess {
// An enum used to designate the type of a piece
enum class PieceType : char {
    none = '-',
    pawn = 'p',
    knight = 'n',
    bishop = 'b',
    rook = 'r',
    queen = 'q',
    king = 'k',
};

// An enum used to designate the color of a piece
enum class PieceColor : char {
    none,
    white = 'w',
    black = 'b'
};

// A struct used to contain the essential data of a chess piece including its type and color
struct Piece {
  public:
    PieceType type;
    PieceColor color;
    bool can_castle;

  public:
    // Default piece constructor
    inline Piece()
        : type(PieceType::none), color(PieceColor::none), can_castle(false) {}
    // Piece constructor
    inline Piece(PieceType type_, PieceColor color_, bool can_castle_ = false)
        : type(type_), color(color_), can_castle(can_castle_) {}
};

// A 64 long (8x8) array which stores pieces
using board_t = std::array<Piece, 64>;
} // namespace chess
