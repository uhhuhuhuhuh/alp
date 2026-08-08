#pragma once

#include <string>

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

// An enum used to designate the color of a piece and a game's active color
enum class Color {
    white,
    black,
};

inline std::string piece_color_to_str(Color pc) {
    if (pc == Color::white) {
        return "white";
    }
    return "black";
}

// A struct used to contain the essential data of a chess piece including its type and color
struct Piece {
  public:
    PieceType type;
    Color color;
    bool can_castle;

  public:
    // Default piece constructor
    inline Piece() : type(PieceType::none), color(Color::white), can_castle(false) {}
    // Piece constructor
    inline Piece(PieceType type_, Color color_, bool can_castle_ = false)
        : type(type_), color(color_), can_castle(can_castle_) {}
};
} // namespace chess
