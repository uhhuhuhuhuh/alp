#pragma once

#include <string_view>

namespace chess::fen {
constexpr std::string_view starting_pos =
                               "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                           promotion_test = "1b3k2/P6P/8/8/8/8/p6p/1B3K2 b - - 0 1";
}