#include "bitboard.hpp"
#include "fen_pos.hpp"
#include "genmoves.hpp"
#include "piece.hpp"
#include "position.hpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

/*
chess::State *
branch(chess::Position &pos, const std::vector<chess::Move> &moves, chess::State *st) {
    for (const auto &target : moves) {
        chess::State *prev = st;
        bool found = false;
        for (const auto &move : chess::gen_moves<chess::GenType::legal>(pos)) {
            if (move.from == target.from && move.to == target.to) {
                found = true;
                chess::do_move(pos, move, *st);
                break;
            }
        }
        if (!found) {
            std::cerr << "Could not find move\n";
            std::abort();
        }

        st++->prev = prev;
    }

    return st;
}

int main() {
    chess::State st_arr[256];
    chess::State *st = st_arr;
    constexpr std::string_view fen = chess::starting_pos_fen;
    std::optional<chess::Position> pos = chess::fen_parser(fen, *st++);
    if (!pos.has_value()) {
        std::cout << "ERROR: can't parse fen: " << fen << '\n';
        return -1;
    }

    std::vector<chess::Move> moves = {
    };
    st = branch(*pos, moves, st);

    chess::output_pos(*pos, std::cout);
    uint32_t depth = 6;
    uint32_t count = chess::perft(*pos, depth - moves.size(), std::cout);
    std::cout << "Count: " << count << " Perft depth: " << depth - moves.size() << '\n';
}
*/

int main() {
    chess::State st;
    constexpr std::string_view fen = chess::fen::starting_pos;
    std::optional<chess::Position> pos = chess::fen_parser(fen, st);
    if (!pos.has_value()) {
        std::cout << "ERROR: can't parse fen: " << fen << '\n';
        return -1;
    }

    uint32_t count = chess::perft(*pos, 7, std::cout);
    std::cout << "Count: " << count << '\n';
}