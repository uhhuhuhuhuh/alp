#pragma once

#include <chrono>
#include <functional>

#include "genmoves.hpp"
#include "position.hpp"

namespace chess::benchmark {
class Timer {
  public:
    void start();
    void stop();

    double duration();

  private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_end;
};

struct PerftResults {
    uint64_t nodes;
    double time_taken_us;
};

template <bool root>
uint64_t perft_recursive(Position &pos, uint8_t depth, std::function<void(std::string)> output) {
    uint64_t count = 0, nodes = 0;
    const bool leaf = depth == 2;

    auto moves = gen_moves<GenType::legal>(pos);
    for (const Move &move : moves) {
        if (root && depth <= 1) {
            count = 1;
            nodes++;
        } else {
            State st;
            pos.do_move(move, st);
            count = leaf ? gen_moves<GenType::legal>(pos).size()
                         : perft_recursive<false>(pos, depth - 1, output);
            nodes += count;
            pos.undo_move(move);
        }
        if constexpr (root) {
            output(move.format() + ": " + std::to_string(count));
        }
    }

    return nodes;
}

inline PerftResults perft(Position &pos, uint8_t depth, std::function<void(std::string)> output) {
    PerftResults results;
    Timer timer;

    timer.start();
    results.nodes = perft_recursive<true>(pos, depth, output);
    timer.stop();

    output("Nodes searched: " + std::to_string(results.nodes));

    results.time_taken_us = timer.duration();
    double nps = (double)results.nodes / (results.time_taken_us / 1'000'000);
    output("Nodes per second: " + std::format("{:.2f}", nps));

    return results;
}
} // namespace chess::benchmark
