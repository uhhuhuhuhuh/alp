#include "position.hpp"
#include <cstdint>
#include <iostream>
#include <string_view>

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