#include "fen_pos.hpp"
#include "genmoves.hpp"
#include "move.hpp"
#include "position.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

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

class NullBuffer : public std::streambuf {
  public:
    int overflow(int c) { return c; }
};

int testing() {
    std::unordered_map<std::size_t, uint64_t> failed;
    for (std::size_t i = 0; i < chess::fen::test_positions.size(); i++) {
        const auto &test = chess::fen::test_positions[i];
        chess::Position pos;
        chess::State st;
        auto err = pos.parse_fen(test.fen, st);
        if (err != chess::Position::FenParseErr::ok) {
            std::cout << "ERROR: can't parse fen: " << test.fen << '\n';
            std::cout << chess::Position::format_fen_err(err) << '\n';
            return -1;
        }

        NullBuffer null_buffer;
        std::ostream null_stream(&null_buffer);
        auto nodes = chess::perft(pos, test.depth, null_stream);
        if (nodes != test.nodes) {
            failed.insert({i, nodes});
        }
    }

    if (failed.empty()) {
        std::cout << "All tests were successful\n";
        return 0;
    }
    for (const auto &[idx, nodes] : failed) {
        const auto &test = chess::fen::test_positions[idx];
        std::cout << "-----FAILED-----\n";
        std::cout << "FEN:   " << test.fen << '\n';
        std::cout << "Depth: " << (int)test.depth << '\n';
        std::cout << "Nodes: " << test.nodes << '\n';
        std::cout << "Incorrect Nodes: " << nodes << '\n';
    }
    std::cout << "TOTAL FAILED: " << failed.size() << '\n';
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 3) {
        std::cerr << "alp\n";
        std::cerr << "or\n";
        std::cerr << "alp \"fen (or start_pos)\" depth (or a move)\n";
        return -1;
    }

    if (argc == 1) {
        return testing();
    }

    chess::Position pos;
    chess::State st;
    if (std::string_view(argv[1]) == "start_pos") {
        pos.parse_fen(chess::fen::starting_pos, st);
    } else {
        auto err = pos.parse_fen(argv[1], st);
        if (err != chess::Position::FenParseErr::ok) {
            std::cerr << "ERROR: can't parse fen: " << argv[1] << '\n';
            std::cerr << chess::Position::format_fen_err(err) << '\n';
            return -1;
        }
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
        auto moves = chess::gen_moves<chess::GenType::legal>(pos);
        auto move_with_flags = find_move(moves.begin(), moves.end(), move);
        if (!move_with_flags.has_value()) {
            std::cerr << "Not a legal move\n";
            return -1;
        }
        chess::State st;
        pos.do_move(*move_with_flags, st);

        pos.output_as_fen(std::cout);
        return 0;
    }

    pos.output(std::cout);
    auto nodes = chess::perft(pos, depth, std::cout);
    std::cout << "Nodes searched: " << nodes << '\n';
}