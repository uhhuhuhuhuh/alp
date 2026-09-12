#pragma once

#include "bitboard.hpp"
#include "move.hpp"
#include "piece.hpp"
#include "square.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <ostream>
#include <string_view>

namespace chess {
enum CastleRights : uint16_t {
    none = 0,
    white_king = 1,
    white_queen = 2,
    black_king = 4,
    black_queen = 8,
};

class State {
  public:
    static constexpr sqr_t ep_none = -1;

    uint64_t hash = 0ULL;
    Color active_color = Color::white;
    uint8_t castle_rights = CastleRights::none;
    Piece captured_piece;
    // set to ep_none if there is nothing to en passant
    sqr_t ep_sqr = ep_none;
    bb::U64 checkers = 0ULL;
    uint32_t halfmove_clock = 0;

    State *prev = nullptr;

    enum class GameEnd : uint8_t {
        proceed,
        checkmate,
        stalemate,
        dead_pos,
        threefold,
        move50,
    };
    GameEnd game_end_state = GameEnd::proceed;

  public:
    State() = default;

    std::string format_game_end();
};

// a class which holds all the important data used in a game of chess.
class Position {
  public:
    uint16_t game_ply = 0;
    std::array<Piece, 64> board;
    bb::PieceBB bb;

    State *st;

    enum class FenParseErr : uint8_t {
        ok,
        empty,
        bad_rank_file_count,
        bad_piece,
        bad_king_count,
        bad_active_color,
        bad_castling,
        no_castle_rook,
        no_castle_king,
        bad_ep_sqr,
        bad_halfmove,
        past_50_move,
        bad_curr_move,
        unknown_property,
        too_few_properties,
    };

  public:
    Position() = default;
    Position(std::string_view sv, State &st);

    // returns a GameState whose data is constructed off of a string formated in FEN notation
    // returns nothing if the string is not in proper FEN notation
    FenParseErr parse_fen(std::string_view sv, State &st);

    static std::string format_fen_err(FenParseErr err);

    State::GameEnd get_game_end_state(const MoveStack &moves) const;

    // makes a move and flips the active side
    void do_move(const Move &move, State &new_st);
    // undoes a move and flips the active side
    void undo_move(const Move &move);

    // outputs the given position in FEN notation
    void output_as_fen(std::ostream &out) const;

    // outputs the data in the given Position and prints the board from white's perspective into the
    // given ostream
    void output(std::ostream &out) const;
};

// Bulk-counting perft: https://joeyrobert.org/chessprogramming-wiki/Perft.html
// Returns the sum of all the total nodes from each searched depth and branch.
// (the link is an archive of https://www.chessprogramming.org which at the time of writing is down)
uint64_t perft(Position &pos, uint8_t depth, std::ostream &out);
uint64_t perft_map(Position &pos, uint8_t depth, std::map<Move, uint64_t> &map);
}; // namespace chess
