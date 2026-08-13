#pragma once

#include "constbb.hpp"
#include "piece.hpp"
#include "square.hpp"

#include <cstdint>

namespace chess::bb {
using U64 = uint64_t;
using BBColorArray = ColorArray<U64>;

void output_bb(U64 bitboard, std::ostream &out);

U64 get_bishop_attack(sqr_t sqr, U64 occ);
U64 get_rook_attack(sqr_t sqr, U64 occ);

class PieceBB {
  private:
    BBColorArray pawns{};
    BBColorArray knights{};
    BBColorArray bishops{};
    BBColorArray rooks{};
    BBColorArray queens{};
    BBColorArray kings{};
    BBColorArray every_type{};
    bb::U64 occupancy{};

    inline void reset_calculated(Color color) {
        every_type[color] = pawns[color] | knights[color] | bishops[color] | rooks[color] |
                            queens[color] | kings[color];
        occupancy = every_type[Color::white] | every_type[Color::black];
    }

  public:
    inline const BBColorArray &get_pawns() const { return pawns; };
    inline const U64 get_all_pawns() const { return pawns[Color::white] | pawns[Color::black]; };
    inline const BBColorArray &get_knights() const { return knights; };
    inline const U64 get_all_knights() const {
        return knights[Color::white] | knights[Color::black];
    };
    inline const BBColorArray &get_bishops() const { return bishops; };
    inline const U64 get_all_bishops() const {
        return bishops[Color::white] | bishops[Color::black];
    };
    inline const BBColorArray &get_rooks() const { return rooks; };
    inline const U64 get_all_rooks() const { return rooks[Color::white] | rooks[Color::black]; };
    inline const BBColorArray &get_queens() const { return queens; };
    inline const U64 get_all_queens() const { return queens[Color::white] | queens[Color::black]; };
    inline const BBColorArray &get_kings() const { return kings; };
    inline const U64 get_all_kings() const { return kings[Color::white] | kings[Color::black]; };
    inline const BBColorArray &get_every_type() const { return every_type; };
    inline U64 get_occupancy() const { return occupancy; };

    inline void set_pawns(Color color, U64 bitboard) {
        pawns[color] = bitboard;
        reset_calculated(color);
    }
    inline void set_knights(Color color, U64 bitboard) {
        knights[color] = bitboard;
        reset_calculated(color);
    }
    inline void set_bishops(Color color, U64 bitboard) {
        bishops[color] = bitboard;
        reset_calculated(color);
    }
    void set_rooks(Color color, U64 bitboard) {
        rooks[color] = bitboard;
        reset_calculated(color);
    }
    void set_queens(Color color, U64 bitboard) {
        queens[color] = bitboard;
        reset_calculated(color);
    }
    void set_kings(Color color, U64 bitboard) {
        kings[color] = bitboard;
        reset_calculated(color);
    }
    void set_bitboard(PieceType type, Color color, U64 bitboard);
    void set_or_bitboard(PieceType type, Color color, bb::U64 bitboard);
    void set_and_bitboard(PieceType type, Color color, bb::U64 bitboard);
};
} // namespace chess::bb
