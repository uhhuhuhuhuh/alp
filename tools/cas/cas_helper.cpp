#include "engine.hpp"
#include "move.hpp"
#include "position.hpp"

#include <iostream>
#include <optional>
#include <string>
#include <string_view>

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

int main(int argc, char **argv) {
    if (argc > 3) {
        std::cerr << "alp \"fen\" depth (or a move)\n";
        return -1;
    }

    chess::Engine engine;

    auto err = engine.parse_fen(argv[1]);
    if (err != chess::Position::FenParseErr::ok) {
        std::cerr << "ERROR: can't parse fen: " << argv[1] << '\n';
        std::cerr << chess::Position::format_fen_err(err) << '\n';
        return -1;
    }

    uint8_t depth = 0;
    try {
        depth = std::stoul(argv[2]);
    } catch (const std::exception &e) {
        chess::Move move;
        if (!move.parse(argv[2])) {
            std::cerr << "ERROR: can't parse depth or move: " << argv[2] << '\n';
            return -1;
        }
        auto moves = engine.gen_legal_moves();
        auto move_with_flags = find_move(moves.begin(), moves.end(), move);
        if (!move_with_flags.has_value()) {
            std::cerr << "Not a legal move\n";
            return -1;
        }
        chess::State st;
        engine.do_move(*move_with_flags);

        std::cout << engine.pos.format_as_fen() << '\n';
        return 0;
    }

    std::cout << engine.pos.format() << '\n';
    engine.pos.perft(depth, [](auto sv) { std::cout << sv << '\n'; });
}
