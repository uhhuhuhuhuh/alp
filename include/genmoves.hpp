#pragma once

#include "gamestate.hpp"
#include "pos.hpp"

#include <vector>
namespace chess {
void gen_pawn_moves(const GameState &state, pos_t pos, std::vector<Move> &moves);
void gen_knight_moves(const GameState &state, pos_t pos, std::vector<Move> &moves);
void gen_bishop_moves(const GameState &state, pos_t pos, std::vector<Move> &moves);
void gen_rook_moves(const GameState &state, pos_t pos, std::vector<Move> &moves);
void gen_queen_moves(const GameState &state, pos_t pos, std::vector<Move> &moves);
void gen_king_moves(const GameState &state, pos_t pos, std::vector<Move> &moves);
// Appends all legal moves for the active color from 'pos' to 'moves'.
// Does not clear 'moves'.
void gen_legal_moves(const GameState &state, std::vector<Move> &moves);
} // namespace chess
