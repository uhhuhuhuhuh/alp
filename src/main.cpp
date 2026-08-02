#include "gamestate.hpp"
#include <iostream>

int main() {
    std::optional<chess::GameState> state = chess::fen_parser(chess::starting_pos_fen);
    if (!state.has_value()) {
        std::cout << "ERROR: can't parse fen: " << chess::starting_pos_fen << '\n';
        return -1;
    }
    chess::ouput_game_state(state.value(), std::cout);
    /* Correctly prints:
     * +-------------------------------+
     * | r | n | b | q | k | b | n | r |
     * +-------------------------------+
     * | p | p | p | p | p | p | p | p |
     * +-------------------------------+
     * | - | - | - | - | - | - | - | - |
     * +-------------------------------+
     * | - | - | - | - | - | - | - | - |
     * +-------------------------------+
     * | - | - | - | - | - | - | - | - |
     * +-------------------------------+
     * | - | - | - | - | - | - | - | - |
     * +-------------------------------+
     * | P | P | P | P | P | P | P | P |
     * +-------------------------------+
     * | R | N | B | Q | K | B | N | R |
     * +-------------------------------+
     *
     * Active Color: white En Passant pos: -1 Halfmove clock: 0 Move count: 1
     */
}
