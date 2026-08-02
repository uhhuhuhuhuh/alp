#pragma once

#include <cstdint>
#include <string_view>

namespace chess {
// an 8 bit long integer (char) used to represent a 1 dimensional position
using pos_t = int8_t;
// an 8 bit long integer (char) used to represent the row of a 2 dimensional position
using rank_t = int8_t;
// an 8 bit long integer (char) used to represent the collumn of a 2 dimensional position
using file_t = int8_t;

// returns a position based off of the given. returns -1 if invalid
pos_t parse_pos(std::string_view sv);
} // namespace chess
