#pragma once

#include "engine.hpp"
#include "position.hpp"

namespace chess {
class RNGEngine : public IEngine {
  public:
    StateStack st_stack;
    Position pos;
    MoveStack moves;

  public:
    RNGEngine();
    RNGEngine(std::string_view fen, Position::FenParseErr &out_err);
    void operator=(const RNGEngine &) = delete;

    Position::FenParseErr parse_fen(std::string_view fen) override;

    Position &position() override;
    const Position &position() const override;

    const MoveStack &gen_legal_moves() override;

    void do_move(const Move &move) override;
    void undo_move(const Move &move) override;

    int32_t evaluate() const override;

    SearchResult search(std::chrono::milliseconds) override;
};
} // namespace chess
