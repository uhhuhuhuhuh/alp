#pragma once

#include "board.hpp"
#include "move.hpp"
#include "pos.hpp"

#include <cstdint>
#include <optional>
#include <ostream>

namespace chess {
// a struct which holds all the important data used in a game of chess.
struct GameState {
  public:
    board_t board;
    PieceColor active_color = PieceColor::white;
    pos_t en_passant_pos = -1;
    uint32_t halfmove_clock = 0;
    uint32_t on_move = 0;

  public:
    GameState() = default;
};

// The starting position in FEN notation
constexpr std::string_view starting_pos_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// returns a GameState whose data is constructed off of a string formated in FEN notation
// returns nothing if the string is not in proper FEN notation
std::optional<GameState> fen_parser(std::string_view sv);

// ouputs the data in the given GameState and prints the board from white's perspective into the given ostream
void ouput_game_state(const GameState &state, std::ostream &out);

// makes a move and flips the active side
void do_move(GameState &state, const Move &move);
// undoes a move and flips the active side
void undo_move(GameState &state, const Move &move);

// Bulk-counting perft: https://joeyrobert.org/chessprogramming-wiki/Perft.html
// Returns the sum of all the total nodes from each searched depth and branch.
// (the link is an archive of https://www.chessprogramming.org which at the time of writing is down)
uint64_t perft(GameState &state, uint32_t depth);

// Bulk-counting perft: https://joeyrobert.org/chessprogramming-wiki/Perft.html
// Writes the game state and perft results for each searched depth and branch
// to the given ostream, and returns the total number of nodes at the requested depth.
// (the link is an archive of https://www.chessprogramming.org which at the time of writing is down)
uint64_t perft_out(GameState &state, uint32_t depth, std::ostream &out);
}; // namespace chess
