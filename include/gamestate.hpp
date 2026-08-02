#pragma once

#include "board.hpp"
#include "pos.hpp"

#include <cstdint>
#include <optional>
#include <ostream>

namespace chess {
enum class ActiveColor : bool {
    black,
    white,
};

// returns "white" if the given ActiveColor == ActiveColor::white else returns "black"
std::string active_color_to_str(ActiveColor ac);

// a struct which holds all the important data used in a game of chess.
struct GameState {
  public:
    board_t board;
    ActiveColor active_color = ActiveColor::white;
    pos_t en_passant_pos = -1;
    uint32_t halfmove_clock = 0;
    uint32_t on_move = 0;

  public:
    GameState() = default;
};

// The starting position in FEN notation
constexpr std::string_view starting_pos_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// returns an option GameState whose data is constructed off of a string formated in FEN notation
// returns nothing if the string is not in proper FEN notation
std::optional<GameState> fen_parser(std::string_view sv);

// ouputs the data in the given GameState and prints the board from white's perspective into the given ostream
void ouput_game_state(const GameState &state, std::ostream &out);
}; // namespace chess
