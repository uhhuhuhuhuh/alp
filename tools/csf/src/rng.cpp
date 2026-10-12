#include "rng.hpp"
#include "genmoves.hpp"
#include <random>

chess::RNGEngine::RNGEngine() : st_stack{}, pos{st_stack}, moves{} {}
chess::RNGEngine::RNGEngine(std::string_view fen, Position::FenParseErr &out_err)
    : st_stack{}, pos{st_stack}, moves{} {
    out_err = parse_fen(fen);
}

chess::Position::FenParseErr chess::RNGEngine::parse_fen(std::string_view fen) {
    return pos.parse_fen(fen);
}

chess::Position &chess::RNGEngine::position() { return pos; }
const chess::Position &chess::RNGEngine::position() const { return pos; }

const chess::MoveStack &chess::RNGEngine::gen_legal_moves() {
    return moves = gen_moves<GenType::legal>(pos);
}

void chess::RNGEngine::do_move(const Move &move) { pos.do_move(move); }

void chess::RNGEngine::undo_move(const Move &move) { pos.undo_move(move); }

int32_t chess::RNGEngine::evaluate() const { return 0; }

chess::RNGEngine::SearchResult chess::RNGEngine::search(std::chrono::milliseconds) {
    gen_legal_moves();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<std::size_t> dist(0, moves.size() - 1);

    return {.best_move = moves[dist(gen)]};
}
