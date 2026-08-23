#include "genmoves.hpp"
#include "position.hpp"
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

/*
static void branch(
    chess::Position &pos, const std::vector<chess::Move> &do_moves,
    std::array<chess::State, 256> &st, std::size_t start_st_idx
) {
    std::size_t st_idx = start_st_idx;
    for (const chess::Move &wanted : do_moves) {
        std::vector<chess::Move> legal_moves;
        chess::gen_legal_moves(pos, legal_moves);

        auto it = std::ranges::find_if(
            legal_moves.begin(), legal_moves.end(), [&](const chess::Move &move) {
                return move.from == wanted.from && move.to == wanted.to;
            }
        );

        if (it == legal_moves.end()) {
            std::cerr << "Move not found\n";
            exit(1);
        }

        start_st_idx++;

        chess::do_move(pos, *it, st[st_idx]);
    }
}

int main() {
    std::array<chess::State, 256> st;
    constexpr std::string_view fen = chess::starting_pos_fen;
    std::optional<chess::Position> pos = chess::fen_parser(fen, st[0]);
    if (!pos.has_value()) {
        std::cout << "ERROR: can't parse fen: " << fen << '\n';
        return -1;
    }

    // c3b4

    std::vector<chess::Move> moves = {
    };

    branch(*pos, moves, st, 1);
    chess::output_pos(*pos, std::cout);
    chess::output_pos_as_fen(*pos, std::cout);
    uint32_t max = 7;
    uint32_t count = chess::perft(*pos, max - moves.size(), std::cout);
    std::cout << "Count: " << count << '\n';
} */

int main() {
    chess::State st;
    constexpr std::string_view fen = chess::starting_pos_fen;
    std::optional<chess::Position> pos = chess::fen_parser(fen, st);
    if (!pos.has_value()) {
        std::cout << "ERROR: can't parse fen: " << fen << '\n';
        return -1;
    }

    uint32_t count = chess::perft(*pos, 7, std::cout);
    std::cout << "Count: " << count << '\n';
}