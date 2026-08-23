#pragma once

#include "bitboard.hpp"
#include "position.hpp"
#include "square.hpp"

#include <vector>

namespace chess {
// returns a bitboard of pieces attacking the given square
bb::U64 attacks_to_sqr(const Position &pos, sqr_t sqr, Color attacked_color);
// returns a bitboard of pieces attacking the king
bb::U64 attacks_to_king(const Position &pos, Color checked_color);

struct PinBBs {
    bb::U64 pinned = 0;
    bb::U64 pinners = 0;
    std::array<bb::U64, 64> pin_rays{};
};
PinBBs get_pins(const Position &pos, Color color);

void gen_pawn_moves(Position &pos, sqr_t sqr_t, MoveArray &moves);
void gen_knight_moves(Position &pos, sqr_t sqr_t, MoveArray &moves);
void gen_bishop_moves(Position &pos, sqr_t sqr_t, MoveArray &moves);
void gen_rook_moves(Position &pos, sqr_t sqr_t, MoveArray &moves);
void gen_queen_moves(Position &pos, sqr_t sqr_t, MoveArray &moves);
void gen_king_moves(Position &pos, sqr_t sqr_t, MoveArray &moves);
// Appends all legal moves for the active color 'moves'.
// Does not clear 'moves'.
MoveArray gen_legal_moves(Position &pos);
} // namespace chess
