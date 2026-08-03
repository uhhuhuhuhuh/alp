#pragma once

#include "board.hpp"
#include "pos.hpp"

namespace chess {
struct Move {
  public:
    pos_t from;
    pos_t to;
    Piece to_piece;

  public:
    Move() : from(-1), to(-1) {};
    Move(pos_t from_, pos_t to_) : from(from_), to(to_) {};
};
} // namespace chess
