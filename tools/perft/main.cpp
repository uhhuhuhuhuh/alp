#include "engine.hpp"

#include <exception>
#include <iostream>

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cout << argv[0] << " [FEN] depth\n";
        return -1;
    }

    chess::Engine engine;
    auto err = engine.parse_fen(argv[1]);
    if (err != chess::Position::FenParseErr::ok) {
        std::cerr << "Can't parse FEN\n";
        std::cerr << chess::Position::format_fen_err(err) << '\n';
        return -1;
    }

    uint8_t depth = 0;
    try {
        depth = std::stoul(argv[2]);
    } catch (std::exception &e) {
        std::cerr << "Invalid depth\n";
    }

    engine.position().perft(depth, [](auto sv) { std::cout << sv << '\n'; });
}
