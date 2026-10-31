#pragma once

#include <array>
#include <cassert>
#include <cctype>
#include <cstdint>
namespace chess {
// An enum used to designate the type of a piece
enum class PieceType : uint8_t {
    none,
    pawn,
    knight,
    bishop,
    rook,
    queen,
    king,
};
constexpr uint8_t max_PieceType = static_cast<uint8_t>(PieceType::king);

// An enum used to designate the color of a piece and a game's active color
enum class Color : bool {
    white,
    black,
};

// returns the opposite color.
inline Color operator!(Color color) {
    if (color == Color::white) { return Color::black; }
    return Color::white;
}

template <typename T> struct ColorArray {
    std::array<T, 2> data{};

  public:
    constexpr ColorArray() = default;
    constexpr ColorArray(const std::array<T, 2> &arr) : data(arr) {};
    constexpr T &operator[](Color color) { return data[static_cast<std::size_t>(color)]; }
    constexpr const T &operator[](Color color) const {
        return data[static_cast<std::size_t>(color)];
    }
};

// A class used to contain the essential data of a chess piece including its type and color
class Piece {
  public:
    PieceType type;
    Color color;

  public:
    // Default piece constructor
    Piece() : type(PieceType::none), color(Color::white) {}
    // Piece constructor
    Piece(PieceType type_, Color color_) : type(type_), color(color_) {}

    bool operator==(const Piece &other) { return type == other.type && color == other.color; }

    Piece(char c) {
        color = std::isupper(c) ? Color::white : Color::black;

        switch (std::tolower(c)) {
        case 'p': type = PieceType::pawn; break;
        case 'n': type = PieceType::knight; break;
        case 'b': type = PieceType::bishop; break;
        case 'r': type = PieceType::rook; break;
        case 'q': type = PieceType::queen; break;
        case 'k': type = PieceType::king; break;
        default:  type = PieceType::none; break;
        }
    }

    char format() const {
        constexpr char chars[] = "-pnbrqk-PNBRQK";
        return chars[static_cast<int>(type) + (color == Color::white ? 7 : 0)];
    }
};

constexpr std::size_t PieceArray_size = static_cast<uint8_t>(PieceType::king);
// Array indexed via a PieceType
// Can not be indexed with PieceType::none
template <typename T> class PieceArray : std::array<T, PieceArray_size> {
  public:
    constexpr T &operator[](PieceType type) {
        assert(type != PieceType::none);
        return std::array<T, PieceArray_size>::operator[](static_cast<std::size_t>(type) - 1);
    }
    constexpr const T &operator[](PieceType type) const {
        assert(type != PieceType::none);
        return std::array<T, PieceArray_size>::operator[](static_cast<std::size_t>(type) - 1);
    }
};
} // namespace chess
