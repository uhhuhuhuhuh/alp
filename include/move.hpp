#pragma once

#include "square.hpp"

namespace chess {
struct Move {
  public:
    sqr_t from;
    sqr_t to;

  public:
    Move() : from(-1), to(-1) {};
    Move(sqr_t from_, sqr_t to_) : from(from_), to(to_) {};
};
} // namespace chess
