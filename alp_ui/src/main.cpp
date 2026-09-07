#include "fen_pos.hpp"
#include "genmoves.hpp"
#include "move.hpp"
#include "position.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <random>
#include <string_view>
#include <vector>

template <typename citer>
std::optional<chess::Move> find_move(citer begin, citer end, const chess::Move &target) {
    for (citer it = begin; it != end; it++) {
        const auto &move = *it;
        if (move.from != target.from || move.to != target.to ||
            move.promotion_type != target.promotion_type) {
            continue;
        }
        return move;
    }
    return {};
}

chess::Move input_move(const chess::MoveStack &moves) {
    chess::Move move;
    std::string in;

    while (true) {
        std::cout << "Alp: Enter Move: ";
        std::cin >> in;
        if (!move.parse(in)) {
            std::cout << "Invalid Move\n";
            continue;
        }

        auto opt_move = find_move(moves.begin(), moves.end(), move);
        if (!opt_move.has_value()) {
            std::cout << "Illegal Move\n";
            continue;
        }
        return *opt_move;
    }
}

chess::State *
branch(chess::Position &pos, const std::vector<chess::Move> &moves, chess::State *st) {
    for (const auto &target : moves) {
        auto move = find_move(moves.begin(), moves.end(), target);
        if (!move.has_value()) {
            std::cerr << "Could not find move\n";
            std::abort();
        }
        pos.do_move(*move, *st);

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
    std::cout << "End State: " << (st - 1)->format_game_end() << '\n';
    std::size_t i = 0;
    for (chess::State *iter = st_arr; iter != st; iter++, i++) {
        std::cout << i << ": " << iter->hash << '\n';
    }
} */

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

    std::random_device rd;
    std::mt19937 gen(rd());

    chess::MoveStack moves;
    bool is_user_turn = true;
    do {
        moves = chess::gen_moves<chess::GenType::legal>(pos);
        chess::Move move;
        pos.output(std::cout);
        if (is_user_turn) {
            move = input_move(moves);
        } else {
            std::uniform_int_distribution<std::size_t> dist(0, moves.size() - 1);
            move = moves[dist(gen)];
            std::cout << "Randomly done move: " << move.format() << '\n';
        }

        pos.do_move(move, *st++);
        is_user_turn = !is_user_turn;
    } while (!moves.is_empty());
    std::cout << (st - 1)->format_game_end() << "!\n";
}