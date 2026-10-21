/*
------ Compare Search Functions ------
*/

#include "fen_pos.hpp"
#include "position.hpp"

#include "check.hpp"
#include "engine.hpp"
#include "lazy.hpp"
#include "lmr.hpp"
#include "null.hpp"
#include "phase.hpp"
#include "rng.hpp"
#include "see.hpp"
#include "starting.hpp"

#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_map>

enum class EngineID { NONE, CURR, RNG, STARTING, PHASE, LAZY, CHECK, LMR, SEE, NULL_MOVE };

struct EnginePtr {
    std::unique_ptr<chess::IEngine> ptr;
    EngineID id;
    bool is_player_one;
};

EnginePtr get_engine(EngineID id, bool is_player_one) {
    EnginePtr engine_ptr{.id = id, .is_player_one = is_player_one};

    switch (id) {
    case EngineID::NONE:      break;
    case EngineID::RNG:       engine_ptr.ptr = std::make_unique<chess::RNGEngine>(); break;
    case EngineID::CURR:      engine_ptr.ptr = std::make_unique<chess::Engine>(); break;
    case EngineID::STARTING:  engine_ptr.ptr = std::make_unique<chess::StartingEngine>(); break;
    case EngineID::PHASE:     engine_ptr.ptr = std::make_unique<chess::PhaseEngine>(); break;
    case EngineID::LAZY:      engine_ptr.ptr = std::make_unique<chess::LazyEngine>(); break;
    case EngineID::CHECK:     engine_ptr.ptr = std::make_unique<chess::CheckEngine>(); break;
    case EngineID::LMR:       engine_ptr.ptr = std::make_unique<chess::LMREngine>(); break;
    case EngineID::SEE:       engine_ptr.ptr = std::make_unique<chess::SEEEngine>(); break;
    case EngineID::NULL_MOVE: engine_ptr.ptr = std::make_unique<chess::NullEngine>(); break;
    }

    return engine_ptr;
}

EngineID get_engine_id(std::string_view sv) {
    auto str = std::string(sv);
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
                  return !std::isspace(ch);
              }));
    std::unordered_map<std::string_view, EngineID> map = {
        {"curr",  EngineID::CURR     },
        {"rng",   EngineID::RNG      },
        {"start", EngineID::STARTING },
        {"phase", EngineID::PHASE    },
        {"lazy",  EngineID::LAZY     },
        {"check", EngineID::CHECK    },
        {"lmr",   EngineID::LMR      },
        {"see",   EngineID::SEE      },
        {"null",  EngineID::NULL_MOVE},
    };
    auto it = map.find(str);
    if (it == map.end())
        return EngineID::NONE;
    return it->second;
}

struct GameResult {
    EnginePtr *winner;
    std::string start_fen;
    std::string end_fen;
};

GameResult run_game(
    EnginePtr &engine_ptr1, EnginePtr &engine_ptr2, std::chrono::milliseconds time,
    std::mt19937 &gen
) {
    chess::IEngine &engine1 = *engine_ptr1.ptr;
    chess::IEngine &engine2 = *engine_ptr2.ptr;
    GameResult res;

    engine1.parse_fen(chess::fen::starting_pos);
    std::uniform_int_distribution<uint8_t> dist(4, 12);
    uint8_t move_count = dist(gen);
    std::vector<chess::Move> done_moves;
    while (true) {
        bool failed = false;
        for (uint8_t i = 0; i < move_count; i++) {
            auto &moves = engine1.gen_legal_moves();
            if (engine1.position().get_game_end(moves) != chess::GameEnd::proceed) {
                failed = true;
                break;
            }

            std::uniform_int_distribution<std::size_t> dist(0, moves.size() - 1);
            const chess::Move &move = moves[dist(gen)];
            engine1.do_move(move);
            done_moves.push_back(move);
        }
        if (failed) {
            for (std::size_t i = done_moves.size(); i-- > 0;) {
                engine1.undo_move(done_moves[i]);
            }
            done_moves.clear();
            continue;
        }
        break;
    }

    res.start_fen = engine1.position().format_as_fen();
    engine2.parse_fen(res.start_fen);

    std::uniform_int_distribution<uint8_t> dist_bool(0, 1);
    bool is_engine1_white = dist_bool(gen);

    while (true) {
        auto &moves = engine1.gen_legal_moves();
        bool is_active_white = engine1.position().st->active_color == chess::Color::white;
        switch (engine1.position().get_game_end(moves)) {
        case chess::GameEnd::proceed: break;
        case chess::GameEnd::checkmate:
            res.end_fen = engine1.position().format_as_fen();
            res.winner = is_engine1_white == is_active_white ? &engine_ptr2 : &engine_ptr1;
            return res;
        default:
            res.end_fen = engine1.position().format_as_fen();
            res.winner = nullptr;
            return res;
        }

        chess::Move move =
            (is_engine1_white == is_active_white ? engine1.search(time) : engine2.search(time))
                .best_move;
        engine1.do_move(move);
        engine2.do_move(move);
    }
}

int main(int argc, char **argv) {
    if (argc != 6) {
        std::cout << argv[0] << " [engine 1] [engine 2] [# of games] [time in ms] [threads]\n"
                  << "Engines:\n"
                  << "    - rng\n"
                  << "    - curr\n"
                  << "    - start\n"
                  << "    - phase\n"
                  << "    - lazy\n"
                  << "    - check\n"
                  << "    - lmr\n"
                  << "    - see\n"
                  << "    - null\n";

        return 0;
    }

    const EngineID id1 = get_engine_id(argv[1]);
    const EngineID id2 = get_engine_id(argv[2]);

    bool invalid_id = false;
    if (id1 == EngineID::NONE) {
        std::cerr << "Invalid engine (first engine): " << argv[1] << '\n';
        invalid_id = true;
    }
    if (id2 == EngineID::NONE) {
        std::cerr << "Invalid engine (second engine): " << argv[2] << '\n';
        invalid_id = true;
    }
    if (invalid_id)
        return 1;

    // Parse numeric arguments.
    auto parse_integer = [](const char *arg) -> unsigned long long {
        std::string str(arg);
        std::size_t pos = 0;

        const auto value = std::stoull(str, &pos);

        if (pos != str.size())
            throw std::invalid_argument("Trailing characters");

        return value;
    };

    std::uint64_t games;
    std::uint64_t time_ms;
    std::uint64_t requested_threads;

    try {
        games = parse_integer(argv[3]);
        time_ms = parse_integer(argv[4]);
        requested_threads = parse_integer(argv[5]);
    } catch (const std::exception &) {
        std::cerr << "Games, time, and threads must be valid "
                     "non-negative integers.\n";
        return 1;
    }

    if (games == 0) {
        std::cerr << "Number of games must be greater than zero.\n";
        return 1;
    }

    if (time_ms == 0 || time_ms > static_cast<std::uint64_t>(
                                      std::numeric_limits<std::chrono::milliseconds::rep>::max()
                                  )) {
        std::cerr << "Invalid time in milliseconds.\n";
        return 1;
    }

    if (requested_threads == 0) {
        std::cerr << "Number of threads must be greater than zero.\n";
        return 1;
    }

    // Leave one hardware thread free when possible.
    const unsigned hardware_threads = std::thread::hardware_concurrency();

    const std::uint64_t max_threads = std::max(hardware_threads, 2u) - 1;

    // Do not create more workers than there are games.
    const unsigned threads =
        static_cast<unsigned>(std::min({requested_threads, max_threads, games}));

    const auto time = std::chrono::milliseconds(time_ms);

    std::cout << "Match: " << argv[1] << " vs " << argv[2] << '\n'
              << "Games: " << games << '\n'
              << "Time per move: " << time.count() << " ms\n"
              << "Parallel workers: " << threads << "\n\n";

    // Atomic match state shared by all workers.
    std::atomic<std::uint64_t> next_game{0};
    std::atomic<std::uint64_t> wins1{0};
    std::atomic<std::uint64_t> wins2{0};
    std::atomic<std::uint64_t> draws{0};

    std::mutex output_mutex;
    std::vector<std::thread> workers;
    workers.reserve(threads);

    for (unsigned t = 0; t < threads; ++t) {
        workers.emplace_back([&]() {
            // Each worker has an independent random generator.
            std::random_device rd;
            std::mt19937 gen(rd());

            // Each worker owns its own pair of engines.
            EnginePtr engine1 = get_engine(id1, true);
            EnginePtr engine2 = get_engine(id2, false);

            while (true) {
                const std::uint64_t i = next_game.fetch_add(1, std::memory_order_relaxed);

                if (i >= games)
                    break;

                GameResult result = run_game(engine1, engine2, time, gen);

                // Record the result against the original engine IDs.
                if (result.winner == &engine1) {
                    wins1.fetch_add(1, std::memory_order_relaxed);
                } else if (result.winner == &engine2) {
                    wins2.fetch_add(1, std::memory_order_relaxed);
                } else {
                    draws.fetch_add(1, std::memory_order_relaxed);
                }

                // Serialize console output so lines don't interleave.
                std::lock_guard<std::mutex> lock(output_mutex);

                std::cout << "Game " << (i + 1) << '/' << games << ": ";

                if (result.winner == nullptr) {
                    std::cout << "Draw";
                } else {
                    std::cout << (result.winner == &engine1 ? argv[1] : argv[2]) << " wins";
                }
                std::cout << '\n';
                std::cout << "    Start FEN: " << result.start_fen << '\n';
                std::cout << "    End FEN:   " << result.end_fen << '\n';
            }
        });
    }

    // Wait for all games to finish.
    for (auto &worker : workers)
        worker.join();

    const auto final_wins1 = wins1.load();
    const auto final_wins2 = wins2.load();
    const auto final_draws = draws.load();

    const double total = static_cast<double>(games);

    std::cout << "\n========== Match Results ==========\n"
              << argv[1] << " wins: " << final_wins1 << " (" << 100.0 * final_wins1 / total
              << "%)\n"
              << argv[2] << " wins: " << final_wins2 << " (" << 100.0 * final_wins2 / total
              << "%)\n"
              << "Draws: " << final_draws << " (" << 100.0 * final_draws / total << "%)\n"
              << "===================================\n";

    return 0;
}
