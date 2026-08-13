#pragma once

#include "position.hpp"
#include "square.hpp"

#include <vector>

namespace chess {
// returns a bitboard of pieces attacking the given square
bb::U64 attacks_to_sqr(const Position &pos, sqr_t sqr, Color attacked_color);
// returns a bitboard of pieces attacking the king
bb::U64 attacks_to_king(const Position &pos, Color checked_color);

void gen_pawn_moves(Position &pos, sqr_t sqr_t, std::vector<Move> &moves);
void gen_knight_moves(Position &pos, sqr_t sqr_t, std::vector<Move> &moves);
void gen_bishop_moves(Position &pos, sqr_t sqr_t, std::vector<Move> &moves);
void gen_rook_moves(Position &pos, sqr_t sqr_t, std::vector<Move> &moves);
void gen_queen_moves(Position &pos, sqr_t sqr_t, std::vector<Move> &moves);
void gen_king_moves(Position &pos, sqr_t sqr_t, std::vector<Move> &moves);
// Appends all legal moves for the active color 'moves'.
// Does not clear 'moves'.
void gen_legal_moves(Position &pos, std::vector<Move> &moves);
} // namespace chess
