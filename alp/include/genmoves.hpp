#pragma once

#include "position.hpp"
#include "square.hpp"

namespace chess {
enum class GenType { pseudo, legal };
// returns a bitboard of pieces attacking the given square
bb::U64 attacks_to_sqr(const Position &pos, sqr_t sqr, Color attacked_color);
// returns a bitboard of pieces attacking the king
bb::U64 attacks_to_king(const Position &pos, Color checked_color);

struct LegalCtx {
    const PinBB &pin_bb;
    sqr_t king_sqr;
    sqr_t checker_sqr;
    bb::U64 evasion_mask;
    bool in_check;
};

template <GenType gen_type>
void gen_pawn_moves_bulk(Position &pos, MoveStack<> &moves, const LegalCtx &ctx);
template <GenType gen_type>
void gen_knight_moves(Position &pos, sqr_t sqr_t, MoveStack<> &moves, const LegalCtx &ctx);
template <GenType gen_type>
void gen_bishop_moves(Position &pos, sqr_t sqr_t, MoveStack<> &moves, const LegalCtx &ctx);
template <GenType gen_type>
void gen_rook_moves(Position &pos, sqr_t sqr_t, MoveStack<> &moves, const LegalCtx &ctx);
template <GenType gen_type>
void gen_queen_moves(Position &pos, sqr_t sqr_t, MoveStack<> &moves, const LegalCtx &ctx);
template <GenType gen_type> void gen_king_moves(Position &pos, sqr_t sqr_t, MoveStack<> &moves);
// Appends all legal moves for the active color 'moves'.
// Does not clear 'moves'.
template <GenType gen_type> MoveStack<> gen_moves(Position &pos);
} // namespace chess
