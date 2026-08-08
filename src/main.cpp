#include "gamestate.hpp"
#include <iostream>

int main() {
    std::optional<chess::GameState> state = chess::fen_parser(chess::starting_pos_fen);
    if (!state.has_value()) {
        std::cout << "ERROR: can't parse fen: " << chess::starting_pos_fen << '\n';
        return -1;
    }

    // chess::ouput_game_state(*state, std::cout);

    // extremely bulky output so instead output the text into a file: ./alp > test.txt
    chess::perft_out(*state, 3, std::cout);

    // for (const auto &bitboard : chess::bb::queen_masks) {
    //     chess::bb::out_bitboard(bitboard, std::cout);
    // }

    // chess::bb::out_bitboard(state->bb.get_all_pawns(), std::cout);
    // chess::bb::out_bitboard(state->bb.get_all_knights(), std::cout);
    // chess::bb::out_bitboard(state->bb.get_all_bishops(), std::cout);
    // chess::bb::out_bitboard(state->bb.get_all_rooks(), std::cout);
    // chess::bb::out_bitboard(state->bb.get_all_queens(), std::cout);
    // chess::bb::out_bitboard(state->bb.get_all_kings(), std::cout);

    // std::random_device rd;
    // std::mt19937 gen(rd());
    // std::uniform_int_distribution<std::size_t> dist1(0, chess::bb::rook_tbl.size() - 1);
    // std::size_t idx = dist1(gen);
    // chess::bb::U64 mask = chess::bb::rook_tbl[idx].mask;
    //
    // std::vector<chess::bb::U64> subsets;
    // for (chess::bb::U64 subset = mask;; subset = (subset - 1) & mask) {
    //     subsets.push_back(subset);
    //     if (subset == 0) {
    //         break;
    //     }
    // }
    // std::uniform_int_distribution<std::size_t> dist2(0, subsets.size() - 1);
    // chess::bb::U64 subset = subsets[dist2(gen)];
    //
    // std::cout << "From: " << chess::pos_format(idx) << '\n';
    // chess::bb::out_bitboard(subset, std::cout);
    // chess::bb::out_bitboard(chess::bb::get_rook_attack(idx, subset), std::cout);
}
