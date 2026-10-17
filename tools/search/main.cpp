#include "engine.hpp"
#include "fen_pos.hpp"

#include <chrono>
#include <exception>
#include <iostream>

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cout << argv[0] << " [FEN] time ms\n";
        return -1;
    }

    chess::Engine engine;
    if (std::string(argv[1]) == "start") {
        engine.parse_fen(chess::fen::starting_pos);
    } else {
        auto err = engine.parse_fen(argv[1]);
        if (err != chess::Position::FenParseErr::ok) {
            std::cerr << "Can't parse FEN\n";
            std::cerr << chess::Position::format_fen_err(err) << '\n';
            return -1;
        }
    }

    using namespace std::chrono_literals;
    std::chrono::milliseconds time = 0ms;
    try {
        time = std::chrono::milliseconds(std::stoul(argv[2]));
    } catch (std::exception &e) {
        std::cerr << "Invalid depth\n";
    }

    auto res = engine.search(time);
    std::cout << "Best Move: " << res.best_move.format() << '\n';
    std::cout << "Depth Went: " << res.depth << '\n';
    std::cout << "Score: " << res.score << '\n';
}
