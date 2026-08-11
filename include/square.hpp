#pragma once

#include <string>
#include <string_view>

namespace chess {
using sqr_t = int;
using rank_t = int;
using file_t = int;

// returns a position based off of 'sv'. ex. sv: e2 returns 12
// returns -1 if invalid
sqr_t parse_sqr(std::string_view sv);

// returns a formated position as a string. ex pos: 12 returns e2
// if the position is out of bounds (pos < 0 or pos >= 64) the function returns the number pos is
// storing as a string.
std::string sqr_format(sqr_t pos);
} // namespace chess
