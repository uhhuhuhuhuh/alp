#pragma once

#include "bitboard.hpp"
#include "move.hpp"
#include "piece.hpp"
#include "square.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <ostream>
#include <vector>

namespace chess {
enum class CastleRights : unsigned int {
    none = 0,
    white_queen = 1,
    white_king = 2,
    black_queen = 4,
    black_king = 8,
};

inline void operator|=(CastleRights &l, CastleRights r) {
    l = static_cast<CastleRights>(static_cast<uint>(l) | static_cast<uint>(r));
}

inline void operator&=(CastleRights &l, CastleRights r) {
    l = static_cast<CastleRights>(static_cast<uint>(l) & static_cast<uint>(r));
}

inline CastleRights operator-(CastleRights l, int r) {
    return static_cast<CastleRights>(static_cast<uint>(l) - r);
}

struct State {
  public:
    Color active_color = Color::white;
    CastleRights castle_rights = CastleRights::none;
    Piece captured_piece;
    sqr_t en_passant_sqr = -1;
    uint32_t ply = 0;
    uint32_t halfmove_clock = 0;
    uint32_t on_move = 0;

  public:
    State() = default;
};

// a struct which holds all the important data used in a game of chess.
struct Position {
  public:
    std::array<Piece, 64> board;
    bb::PieceBB bb;

    State state;

    std::vector<State> history;

  public:
    Position() = default;
};

// outputs the given position in FEN notation
void output_pos_as_fen(const Position &pos, std::ostream &out);

// The starting position in FEN notation
constexpr std::string_view starting_pos_fen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// returns a GameState whose data is constructed off of a string formated in FEN notation
// returns nothing if the string is not in proper FEN notation
std::optional<Position> fen_parser(std::string_view sv);

// ouputs the data in the given Position and prints the board from white's perspective into the
// given ostream
void ouput_pos(const Position &pos, std::ostream &out);

// makes a move and flips the active side
void do_move(Position &pos, const Move &move);
// undoes a move and flips the active side
void undo_move(Position &pos, const Move &move);

// Bulk-counting perft: https://joeyrobert.org/chessprogramming-wiki/Perft.html
// Returns the sum of all the total nodes from each searched depth and branch.
// (the link is an archive of https://www.chessprogramming.org which at the time of writing is down)
uint64_t perft(Position &pos, uint32_t depth);

// Bulk-counting perft: https://joeyrobert.org/chessprogramming-wiki/Perft.html
// Writes the positon and perft results for each searched depth and branch
// to the given ostream, and returns the total number of nodes at the requested depth.
// (the link is an archive of https://www.chessprogramming.org which at the time of writing is down)
uint64_t perft_out(Position &pos, uint32_t depth, std::ostream &out);
}; // namespace chess
