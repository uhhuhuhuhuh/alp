#pragma once

#include <string>
#include <string_view>

namespace chess {
using sqr_t = int8_t;
using rank_t = int8_t;
using file_t = int8_t;
using rf_t = int8_t;

enum File { A, B, C, D, E, F, G, H };
enum Rank { R1, R2, R3, R4, R5, R6, R7, R8 };

// clang-format off
enum Sqr : sqr_t {
	a1, b1, c1, d1, e1, f1, g1, h1,
	a2, b2, c2, d2, e2, f2, g2, h2,
	a3, b3, c3, d3, e3, f3, g3, h3,
	a4, b4, c4, d4, e4, f4, g4, h4,
	a5, b5, c5, d5, e5, f5, g5, h5,
	a6, b6, c6, d6, e6, f6, g6, h6,
	a7, b7, c7, d7, e7, f7, g7, h7,
	a8, b8, c8, d8, e8, f8, g8, h8,
};
// clang-format on

// returns a position based off of 'sv'. ex. sv: e2 returns 12
// returns -1 if invalid
sqr_t parse_sqr(std::string_view sv);

// returns a formated position as a string. ex sqr: 12 returns e2
// if the position is out of bounds (sqr < 0 or sqr >= 64) the function returns the number sqr is
// storing as a string.
std::string format_sqr(sqr_t sqr);
} // namespace chess
