#pragma once

#include <bit>

#include "piece_color.hpp"
#include "square.hpp"

namespace chess {
template <typename T> inline auto popcount(T x) { return std::popcount(x); }
template <typename T> inline auto lsb(T x) { return std::countr_zero(x); }

// returns index of least significant bit.
template <typename intn_t> inline std::size_t poplsb(intn_t &x) {
    std::size_t index = lsb(x);
    x &= x - 1;
    return index;
}

constexpr sqr_t rel_sqr(sqr_t sqr, Color color) { return color == Color::white ? sqr : sqr ^ 56; }
} // namespace chess
