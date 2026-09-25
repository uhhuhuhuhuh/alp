#pragma once

#include "position.hpp"
#include <bit>

namespace chess {
// returns index of least significant bit.
template <typename intn_t> inline std::size_t poplsb(intn_t &x) {
    std::size_t index = std::countr_zero(x);
    x &= x - 1;
    return index;
}

inline bb::U64 attacks_to_sqr(const Position &pos, sqr_t sqr, Color color) {
    Color enemy = !color;

    bb::U64 occupancy = pos.bb.occupancy();
    // Rook & Queen, Bishop & Queen
    bb::U64 RQ, BQ;
    RQ = BQ = pos.bb.get(PieceType::queen)[enemy];
    RQ |= pos.bb.get(PieceType::rook)[enemy];
    BQ |= pos.bb.get(PieceType::bishop)[enemy];

    return (bb::pawn_attacks[color][sqr] & pos.bb.get(PieceType::pawn)[enemy]) |
           (bb::knight_attacks[sqr] & pos.bb.get(PieceType::knight)[enemy]) |
           (bb::get_bishop_attack(sqr, occupancy) & BQ) |
           (bb::get_rook_attack(sqr, occupancy) & RQ) |
           (bb::king_attacks[sqr] & pos.bb.get(PieceType::king)[enemy]);
}

struct PinBBs {
    bb::U64 pinned = 0;
    bb::U64 pinners = 0;
    std::array<bb::U64, 64> pin_rays{};
};
inline PinBBs get_pins(const Position &pos, Color color) {
    PinBBs pin_bbs;
    bb::U64 occ = pos.bb.occupancy();
    bb::U64 color_occ = pos.bb.every_type()[color];
    bb::U64 king_bb = pos.bb.get(PieceType::king)[color];
    assert(std::popcount(king_bb) == 1);
    sqr_t king_sqr = std::countr_zero(king_bb);

    bb::U64 queen_bb = pos.bb.get(PieceType::queen)[!color];

    bb::U64 pinner = bb::xray_bishop_attack(occ, color_occ, king_sqr) &
                     (pos.bb.get(PieceType::bishop)[!color] | queen_bb);
    pin_bbs.pinners |= pinner;
    while (pinner != 0) {
        sqr_t sqr = poplsb(pinner);
        bb::U64 between = bb::slider_between[sqr][king_sqr];
        sqr_t pinned_sqr = std::countr_zero(between & color_occ);
        pin_bbs.pin_rays[pinned_sqr] = between | (1ULL << sqr);
        pin_bbs.pinned |= 1ULL << pinned_sqr;
    }

    pinner = bb::xray_rook_attack(occ, color_occ, king_sqr) &
             (pos.bb.get(PieceType::rook)[!color] | queen_bb);
    pin_bbs.pinners |= pinner;
    while (pinner != 0) {
        sqr_t sqr = poplsb(pinner);
        bb::U64 between = bb::slider_between[sqr][king_sqr];
        sqr_t pinned_sqr = std::countr_zero(between & color_occ);
        pin_bbs.pin_rays[pinned_sqr] = between | (1ULL << sqr);
        pin_bbs.pinned |= 1ULL << pinned_sqr;
    }

    return pin_bbs;
}
} // namespace chess
