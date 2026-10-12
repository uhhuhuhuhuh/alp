#pragma once

#include <bit>

namespace chess {
template <typename T> inline auto popcount(T x) { return std::popcount(x); }
template <typename T> inline auto lsb(T x) { return std::countr_zero(x); }

// returns index of least significant bit.
template <typename intn_t> inline std::size_t poplsb(intn_t &x) {
    std::size_t index = lsb(x);
    x &= x - 1;
    return index;
}
} // namespace chess
