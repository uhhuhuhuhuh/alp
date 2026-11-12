#pragma once

#include "benchmarking.hpp"
#include "bitboard.hpp"
#include "move.hpp"
#include "piece_color.hpp"
#include "square.hpp"
#include "state.hpp"

#include <array>
#include <cstdint>
#include <stop_token>
#include <string_view>

namespace chess {
struct PinBB {
    bb::U64 pinned = 0;
    bb::U64 pinners = 0;
    std::array<bb::U64, 64> pin_rays{};
};

enum class GameEnd : uint8_t { proceed, checkmate, stalemate, dead_pos, threefold, move50 };
std::string format_game_end(GameEnd game_end);

// a class which holds all the important data used in a game of chess.
class Position {
  public:
    static constexpr uint16_t invalid_game_ply = std::numeric_limits<uint16_t>().max();
    uint16_t game_ply = invalid_game_ply;
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
        bad_ep_rank,
        ep_not_empty,
        no_ep_pawn,
        bad_halfmove,
        past_50_move,
        bad_curr_move,
        unknown_property,
        too_few_properties,
    };

    struct PerftResults {
        uint64_t nodes;
        double time_taken_us;
    };

  private:
    MoveStack<> gen_legal_moves();

    template <bool root, typename output_fn>
    uint64_t perft_recursive(uint8_t depth, output_fn output) {
        uint64_t count = 0, nodes = 0;
        const bool leaf = depth == 2;

        for (const Move &move : gen_legal_moves()) {
            if (root && depth <= 1) {
                count = 1;
                nodes++;
            } else {
                State st;
                do_move(move, st);
                count = leaf ? gen_legal_moves().size() : perft_recursive<false>(depth - 1, output);
                nodes += count;
                undo_move(move);
            }
            if constexpr (root) { output(std::format("{}: {}", move.format_uci(), count)); }
        }

        return nodes;
    }
    template <bool root, typename output_fn>
    uint64_t perft_recursive(uint8_t depth, output_fn output, std::stop_token stop_token) {
        uint64_t count = 0, nodes = 0;
        const bool leaf = depth == 2;

        for (const Move &move : gen_legal_moves()) {
            if (stop_token.stop_requested()) { return nodes; }
            if (root && depth <= 1) {
                count = 1;
                nodes++;
            } else {
                State st;
                do_move(move, st);
                count = leaf ? gen_legal_moves().size()
                             : perft_recursive<false>(depth - 1, output, stop_token);
                nodes += count;
                undo_move(move);
            }
            if constexpr (root) { output(std::format("{}: {}", move.format_uci(), count)); }
        }

        return nodes;
    }

  public:
    Position();
    Position(std::string_view sv, State &st, FenParseErr &out_err);

    // returns a GameState whose data is constructed off of a string formated in FEN notation
    // returns nothing if the string is not in proper FEN notation
    FenParseErr parse_fen(std::string_view sv, State &st);

    static std::string format_fen_err(FenParseErr err);

    GameEnd get_game_end(const MoveStack<> &moves) const;

    // makes a move and flips the active side
    void do_move(const Move &move, State &new_st);
    // undoes a move and flips the active side
    void undo_move(const Move &move);

    // makes a move and flips the active side
    void do_null_move(State &new_st);
    // undoes a move and flips the active side
    void undo_null_move();

    template <typename output_fn>
    PerftResults perft(uint8_t depth, output_fn output, std::stop_token stop_token) {
        PerftResults results;
        chess::benchmark::Timer timer;

        timer.start();
        if (depth == 0) {
            results.nodes = 1;
        } else {
            results.nodes = perft_recursive<true>(depth, output, stop_token);
        }
        timer.stop();
        output(std::format("Nodes searched: {}", results.nodes));

        results.time_taken_us = timer.duration();
        double nps = (double)results.nodes / (results.time_taken_us / 1'000'000);
        output(std::format("Nodes per Second: {:.2f}", nps));

        return results;
    }
    template <typename output_fn> PerftResults perft(uint8_t depth, output_fn output) {
        PerftResults results;
        chess::benchmark::Timer timer;

        timer.start();
        if (depth == 0) {
            results.nodes = 1;
        } else {
            results.nodes = perft_recursive<true>(depth, output);
        }
        timer.stop();
        output(std::format("Nodes searched: {}", results.nodes));

        results.time_taken_us = timer.duration();
        double nps = (double)results.nodes / (results.time_taken_us / 1'000'000);
        output(std::format("Nodes per Second: {:.2f}", nps));

        return results;
    }

    bb::U64 attacks_to_sqr(sqr_t sqr, Color color) const;

    PinBB get_pins(Color color);

    // outputs the given position in FEN notation
    std::string format_as_fen() const;

    // outputs the data in the given Position and prints the board from white's perspective into the
    // given ostream
    std::string format() const;
};
}; // namespace chess
