#include "position.hpp"
#include <iostream>
#include <string_view>

int main() {
    constexpr std::string_view fen = chess::starting_pos_fen;
    std::optional<chess::Position> pos = chess::fen_parser(fen);
    if (!pos.has_value()) {
        std::cout << "ERROR: can't parse fen: " << fen << '\n';
        return -1;
    }

    // extremely bulky output so instead output the text into a file: ./alp > test.txt
    // chess::perft_out(*pos, 5, std::cout);

    std::cout << chess::perft(*pos, 7) << '\n';
}
