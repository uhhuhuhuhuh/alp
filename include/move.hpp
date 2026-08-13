#pragma once

#include "square.hpp"

namespace chess {
enum MoveFlag {
    quiet,
    capture,
    ep,
    castle,
    promotion,
};

struct Move {
  public:
    sqr_t from;
    sqr_t to;
    int8_t flags;

  public:
    Move() : from(-1), to(-1), flags(MoveFlag::quiet) {};
    Move(sqr_t from_, sqr_t to_) : from(from_), to(to_), flags(MoveFlag::quiet) {};
};
} // namespace chess
