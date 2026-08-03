#include "gamestate.hpp"
#include <iostream>

int main() {
    std::optional<chess::GameState> state = chess::fen_parser(chess::starting_pos_fen);
    if (!state.has_value()) {
        std::cout << "ERROR: can't parse fen: " << chess::starting_pos_fen << '\n';
        return -1;
    }
    // chess::ouput_game_state(state.value(), std::cout);
    // std::cout << chess::perft(state.value(), 2) << '\n';

    // extremely bulky output so instead output the text into a file: ./alp > test.txt
    chess::perft_out(state.value(), 3, std::cout);
}
