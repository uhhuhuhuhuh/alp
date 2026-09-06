#include "bitboard.hpp"
#include "constbb.hpp"
#include "fen_pos.hpp"
#include "genmoves.hpp"
#include "position.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

chess::State *
branch(chess::Position &pos, const std::vector<chess::Move> &moves, chess::State *st) {
    for (const auto &target : moves) {
        bool found = false;
        for (const auto &move : chess::gen_moves<chess::GenType::legal>(pos)) {
            if (move.from != target.from || move.to != target.to) {
                continue;
            }
            found = true;
            pos.do_move(move, *st);
            break;
        }
        if (!found) {
            std::cerr << "Could not find move\n";
            std::abort();
        }

        st++;
    }

    return st;
}

/*
int main() {
    chess::State st_arr[256];
    chess::State *st = st_arr;
    constexpr std::string_view fen = chess::fen::starting_pos;
    chess::Position pos;
    auto err = pos.parse_fen(fen, *st++);
    if (err != chess::Position::FenParseErr::ok) {
        std::cout << "ERROR: can't parse fen: " << fen << '\n';
        std::cout << chess::Position::format_fen_err(err) << '\n';
        return -1;
    }

    std::vector<chess::Move> moves = {
        chess::Move{chess::g1, chess::f3},
        chess::Move{chess::g8, chess::f6},
        chess::Move{chess::f3, chess::g1},
        chess::Move{chess::f6, chess::g8},
        chess::Move{chess::g1, chess::f3},
        chess::Move{chess::g8, chess::f6},
        chess::Move{chess::f3, chess::g1},
        chess::Move{chess::f6, chess::g8},
    };
    st = branch(pos, moves, st);
    std::cout << "count: " << chess::gen_moves<chess::GenType::legal>(pos).size() << '\n';
    std::cout << "Is Threefold: " << (pos.st->game_end_state == chess::State::GameEnd::threefold)
              << '\n';
    std::size_t i = 0;
    for (chess::State *iter = st_arr; iter != st; iter++, i++) {
        std::cout << i << ": " << iter->hash << '\n';
    }
} */

int main() {
    chess::State st;
    constexpr std::string_view fen = chess::fen::starting_pos;
    chess::Position pos;
    auto err = pos.parse_fen(fen, st);
    if (err != chess::Position::FenParseErr::ok) {
        std::cout << "ERROR: can't parse fen: " << fen << '\n';
        std::cout << chess::Position::format_fen_err(err) << '\n';
        return -1;
    }

    auto count = chess::perft(pos, 7, std::cout);
    std::cout << "Count: " << count << '\n';
}