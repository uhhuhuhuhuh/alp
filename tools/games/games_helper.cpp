#include "engine.hpp"
#include "fen_pos.hpp"
#include "move.hpp"
#include "position.hpp"

#include <chrono>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

std::random_device rd;
std::mt19937 gen(rd());

template <typename citer>
std::optional<chess::Move> find_move(citer begin, citer end, const chess::Move &target) {
    for (citer it = begin; it != end; ++it) {
        const auto &move = *it;

        if (move.from == target.from && move.to == target.to &&
            move.promotion_type == target.promotion_type) {
            return move;
        }
    }

    return {};
}

std::string process_request(chess::Engine &engine, const std::string &arg) {
    if (arg == "f") { return engine.pos.format_as_fen(); }

    if (arg == "p") {
        engine.parse_fen(chess::fen::starting_pos);
        return engine.pos.format_as_fen();
    }

    if (arg == "e") {
        auto game_end = engine.pos.get_game_end(engine.gen_legal_moves());

        switch (game_end) {
        case chess::GameEnd::proceed:   return "ok";
        case chess::GameEnd::checkmate: return "mate";
        default:                        return "draw";
        }
    }

    uint16_t time_ms = 0;

    try {
        time_ms = std::stoul(arg);
    } catch (const std::exception &) {
        chess::Move move;

        if (!move.parse_uci(arg)) { return "ERROR: can't parse time or move"; }

        auto &moves = engine.gen_legal_moves();
        auto found = find_move(moves.begin(), moves.end(), move);

        if (!found.has_value()) { return "ERROR: Not a legal move"; }

        engine.do_move(*found);
        return engine.pos.format_as_fen();
    }

    return engine.search(std::chrono::milliseconds(time_ms)).best_move.format();
}

int main() {
    chess::Engine engine;
    std::string line;

    while (std::getline(std::cin, line)) {
        try {
            std::cout << process_request(engine, line) << std::endl;
        } catch (const std::exception &e) {
            std::cerr << "Helper error: " << e.what() << '\n';
            std::cout << "ERROR: helper exception" << std::endl;
        }
    }

    return 0;
}
