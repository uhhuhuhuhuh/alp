#include "engine.hpp"
#include "position.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct CachedPosition {
    std::array<chess::Piece, 64> board;
    chess::bb::PieceBB bb;
    uint16_t game_ply;
    chess::State state;
    double result;
};

struct Sample {
    std::string fen;
    double result; // 1.0 = White wins, 0.5 = draw, 0.0 = Black wins
};

using Settings = chess::Engine::EvalSettings;

constexpr double kScalingFactor = 0.4;
constexpr double kImprovementThreshold = 1e-12;

struct Progress {
    using Clock = std::chrono::steady_clock;

    std::size_t completed = 0;
    std::size_t total = 0;
    std::size_t pass = 0;
    int step = 0;
    std::size_t step_index = 0;
    std::size_t step_count = 0;

    Clock::time_point run_start = Clock::now();
    Clock::time_point pass_start = Clock::now();

    static double seconds(Clock::duration duration) {
        return std::chrono::duration<double>(duration).count();
    }

    static std::string format_time(double value) {
        if (!std::isfinite(value) || value < 0.0) return "--:--";

        const auto total_seconds = static_cast<std::uint64_t>(value);

        const auto hours = total_seconds / 3600;
        const auto minutes = (total_seconds / 60) % 60;
        const auto secs = total_seconds % 60;

        std::ostringstream out;
        if (hours > 0) out << hours << ':' << std::setw(2) << std::setfill('0') << minutes << ':';
        else
            out << minutes << ':';

        out << std::setw(2) << std::setfill('0') << secs;
        return out.str();
    }

    void start_run() { run_start = Clock::now(); }

    void reset(
        std::size_t count, int step_size, std::size_t pass_number, std::size_t current_step,
        std::size_t total_steps, double best_loss
    ) {
        completed = 0;
        total = count;
        step = step_size;
        pass = pass_number;
        step_index = current_step;
        step_count = total_steps;
        pass_start = Clock::now();
        print(best_loss);
    }

    void update(double best_loss) {
        ++completed;

        const double elapsed = seconds(Clock::now() - pass_start);
        const double remaining = completed > 0 ? elapsed / static_cast<double>(completed) *
                                                     static_cast<double>(total - completed)
                                               : 0.0;

        print(best_loss, remaining);
    }

    void print(double loss, double eta = 0.0) const {
        constexpr std::size_t bar_width = 30;

        const double fraction =
            total == 0 ? 1.0 : static_cast<double>(completed) / static_cast<double>(total);

        const auto filled = static_cast<std::size_t>(fraction * static_cast<double>(bar_width));

        const double elapsed = seconds(Clock::now() - run_start);
        const double percent = fraction * 100.0;

        std::cout << '\r' << "Step " << step_index << '/' << step_count << " | Size "
                  << std::setw(2) << step << " | Pass " << pass << " | [";

        for (std::size_t i = 0; i < bar_width; ++i) std::cout << (i < filled ? '#' : '-');

        std::cout << "] " << std::fixed << std::setprecision(1) << std::setw(5) << percent << "% "
                  << '(' << completed << '/' << total << ')' << " | Loss " << std::setprecision(10)
                  << loss << " | Elapsed " << format_time(elapsed) << " | Pass ETA "
                  << format_time(eta) << std::flush;
    }

    void finish(double best_loss) {
        completed = total;
        print(best_loss, 0.0);
        std::cout << '\n';
    }
};

// Load positions from a TSV file containing:
// FEN<TAB>result
std::vector<CachedPosition> load_positions(const std::string &path, chess::Engine &engine) {
    std::ifstream file(path);
    if (!file) { throw std::runtime_error("Could not open " + path); }

    std::vector<CachedPosition> positions;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') { continue; }

        const auto tab_position = line.rfind('\t');

        if (tab_position == std::string::npos) {
            throw std::runtime_error("Expected FEN<TAB>result");
        }

        const std::string fen = line.substr(0, tab_position);
        const double result = std::stod(line.substr(tab_position + 1));

        if (result < 0.0 || result > 1.0) {
            throw std::runtime_error("Result must be between 0 and 1");
        }

        if (engine.parse_fen(fen) != chess::Position::FenParseErr::ok) {
            throw std::runtime_error("Failed to parse FEN: " + fen);
        }

        positions.push_back(
            {.board = engine.pos.board,
             .bb = engine.pos.bb,
             .game_ply = engine.pos.game_ply,
             .state = *engine.pos.st,
             .result = result}
        );
    }

    if (positions.empty()) { throw std::runtime_error("Dataset is empty"); }

    return positions;
}

double predict_white_score(double evaluation) {
    constexpr double ln_10 = 2.30258509299f;
    const double exponent = -kScalingFactor * ln_10 * evaluation / 400.0;
    if (exponent >= 0.0) {
        const double exp_negative = std::exp(-exponent);
        return exp_negative / (1.0 + exp_negative);
    }
    return 1.0 / (1.0 + std::exp(exponent));
}

double calculate_loss(
    chess::Engine &engine, const std::vector<CachedPosition> &positions, const Settings &settings
) {
    double total_squared_error = 0.0;

    for (const auto &cached : positions) {
        engine.pos.board = cached.board;
        engine.pos.bb = cached.bb;
        engine.pos.game_ply = cached.game_ply;
        *engine.pos.st = cached.state;
        engine.pos.st->prev = nullptr;

        double evaluation = engine.evaluate(settings);
        if (cached.state.active_color == chess::Color::black) { evaluation = -evaluation; }

        const double prediction = predict_white_score(evaluation);
        const double difference = prediction - cached.result;

        total_squared_error += difference * difference;
    }

    return total_squared_error / static_cast<double>(positions.size());
}

struct Range {
    int32_t min = 0;
    int32_t max = 0;
    Range() = default;
    Range(int32_t min, int32_t max) : min(min), max(max) {}
};

struct Parameter {
    std::string name;
    int32_t Settings::*member = nullptr;
    Range range;

    bool new_group = false;

    std::optional<std::size_t> rook_adj_idx{};
    std::optional<std::size_t> knight_adj_idx{};

    std::optional<chess::PieceType> pst_piece{};
    std::optional<std::size_t> pst_square{};
    bool pst_endgame = false;
};

const std::array<Parameter, 26> kBaseParameters{
    {
     {.name = "queen_val", .member = &Settings::queen_val, .range = {700, 1200}},
     {.name = "rook_val", .member = &Settings::rook_val, .range = {350, 700}},
     {.name = "bishop_val", .member = &Settings::bishop_val, .range = {150, 450}},
     {.name = "knight_val", .member = &Settings::knight_val, .range = {150, 450}},

     {.name = "blocked_penalty",
         .member = &Settings::blocked_penalty,
         .range = {0, 150},
         .new_group = true},
     {.name = "doubled_penalty", .member = &Settings::doubled_penalty, .range = {0, 150}},
     {.name = "isolated_penalty", .member = &Settings::isolated_penalty, .range = {0, 150}},
     {.name = "backward_penalty", .member = &Settings::backward_penalty, .range = {0, 150}},

     {.name = "passed_bonus",
         .member = &Settings::passed_bonus,
         .range = {0, 150},
         .new_group = true},

     {.name = "king_shield_2_bonus",
         .member = &Settings::king_shield_2_bonus,
         .range = {0, 150},
         .new_group = true},
     {.name = "king_shield_3_bonus",
         .member = &Settings::king_shield_3_bonus,
         .range = {0, 150}},

     {.name = "mobility_0_knight_penalty",
         .member = &Settings::mobility_0_knight_penalty,
         .range = {0, 50},
         .new_group = true},
     {.name = "knight_king_attackers_weight",
         .member = &Settings::knight_king_attackers_weight,
         .range = {0, 100}},

     {.name = "bishop_pair_bonus",
         .member = &Settings::bishop_pair_bonus,
         .range = {0, 150},
         .new_group = true},
     {.name = "returning_bishop_bonus",
         .member = &Settings::returning_bishop_bonus,
         .range = {0, 150}},
     {.name = "mobility_0_bishop_penalty",
         .member = &Settings::mobility_0_bishop_penalty,
         .range = {0, 50}},
     {.name = "bishop_king_attackers_weight",
         .member = &Settings::bishop_king_attackers_weight,
         .range = {0, 100}},

     {.name = "mobility_0_rook_penalty",
         .member = &Settings::mobility_0_rook_penalty,
         .range = {0, 50},
         .new_group = true},
     {.name = "mg_rook_7th_bonus", .member = &Settings::mg_rook_7th_bonus, .range = {0, 50}},
     {.name = "eg_rook_7th_bonus", .member = &Settings::eg_rook_7th_bonus, .range = {0, 50}},
     {.name = "rook_full_open_file_bonus",
         .member = &Settings::rook_full_open_file_bonus,
         .range = {0, 50}},
     {.name = "rook_half_open_file_bonus",
         .member = &Settings::rook_half_open_file_bonus,
         .range = {0, 50}},
     {.name = "rook_king_attackers_weight",
         .member = &Settings::rook_king_attackers_weight,
         .range = {0, 100}},

     {.name = "early_developed_queen_penalty",
         .member = &Settings::early_developed_queen_penalty,
         .range = {0, 150},
         .new_group = true},
     {.name = "mobility_0_queen_penalty",
         .member = &Settings::mobility_0_queen_penalty,
         .range = {0, 50}},
     {.name = "queen_king_attackers_weight",
         .member = &Settings::queen_king_attackers_weight,
         .range = {0, 100}},
     }
};

constexpr std::array<chess::PieceType, 6> kPieceTypes{
    {
     chess::PieceType::pawn,
     chess::PieceType::knight,
     chess::PieceType::bishop,
     chess::PieceType::rook,
     chess::PieceType::queen,
     chess::PieceType::king,
     }
};

constexpr std::array<const char *, 6> kPieceNames{
    {"pawn", "knight", "bishop", "rook", "queen", "king"}
};

int32_t get_parameter_value(const Settings &settings, const Parameter &parameter) {
    if (parameter.pst_piece.has_value()) {
        const auto &tables = settings.pst[*parameter.pst_piece];
        const std::size_t square = parameter.pst_square.value();

        return parameter.pst_endgame ? tables.endgame[square] : tables.middlegame[square];
    }

    if (parameter.rook_adj_idx.has_value()) { return settings.rook_adj[*parameter.rook_adj_idx]; }

    if (parameter.knight_adj_idx.has_value()) {
        return settings.knight_adj[*parameter.knight_adj_idx];
    }

    return settings.*(parameter.member);
}

void set_parameter_value(Settings &settings, const Parameter &parameter, int32_t value) {
    if (parameter.pst_piece.has_value()) {
        auto &tables = settings.pst[*parameter.pst_piece];
        const std::size_t square = parameter.pst_square.value();

        auto &entry = parameter.pst_endgame ? tables.endgame[square] : tables.middlegame[square];

        entry = static_cast<int16_t>(value);
        return;
    }

    if (parameter.rook_adj_idx.has_value()) {
        settings.rook_adj[*parameter.rook_adj_idx] = value;
        return;
    }

    if (parameter.knight_adj_idx.has_value()) {
        settings.knight_adj[*parameter.knight_adj_idx] = value;
        return;
    }

    settings.*(parameter.member) = value;
}

std::vector<Parameter> make_parameters() {
    std::vector<Parameter> parameters(kBaseParameters.begin(), kBaseParameters.end());

    constexpr int32_t adj_min = -50;
    constexpr int32_t adj_max = 50;
    constexpr std::size_t adj_size = 9;

    for (std::size_t i = 0; i < adj_size; ++i) {
        Parameter rook{};
        rook.name = "rook_adj[" + std::to_string(i) + "]";
        rook.range.min = adj_min;
        rook.range.max = adj_max;
        rook.rook_adj_idx = i;
        parameters.push_back(rook);

        Parameter knight{};
        knight.name = "knight_adj[" + std::to_string(i) + "]";
        knight.range.min = adj_min;
        knight.range.max = adj_max;
        knight.knight_adj_idx = i;
        parameters.push_back(knight);
    }

    for (std::size_t piece_idx = 0; piece_idx < kPieceTypes.size(); ++piece_idx) {
        const auto piece = kPieceTypes[piece_idx];

        for (const bool endgame : {false, true}) {
            for (std::size_t square = 0; square < 64; ++square) {
                Parameter parameter{};

                parameter.name = std::format(
                    "pst[{}].{}[{}]",
                    std::string(kPieceNames[piece_idx]),
                    (endgame ? "endgame" : "middlegame"),
                    std::to_string(square)
                );

                parameter.range.min = -100;
                parameter.range.max = 100;
                parameter.pst_piece = piece;
                parameter.pst_square = square;
                parameter.pst_endgame = endgame;

                parameters.push_back(parameter);
            }
        }
    }

    return parameters;
}

bool optimize_parameters(
    chess::Engine &engine, const std::vector<CachedPosition> &positions, Settings &settings,
    const std::vector<Parameter> &parameters, double &best_loss, int step, Progress &progress
) {
    bool improved = false;

    for (const auto &parameter : parameters) {
        const int32_t original_value = get_parameter_value(settings, parameter);

        int32_t best_value = original_value;

        for (const int sign : {-1, 1}) {
            const int32_t candidate = original_value + sign * step;
            if (candidate < parameter.range.min || candidate > parameter.range.max) { continue; }

            set_parameter_value(settings, parameter, candidate);

            const double candidate_loss = calculate_loss(engine, positions, settings);
            if (candidate_loss < best_loss - kImprovementThreshold) {
                best_loss = candidate_loss;
                best_value = candidate;
            }
        }

        set_parameter_value(settings, parameter, best_value);

        if (best_value != original_value) { improved = true; }

        progress.update(best_loss);
    }

    return improved;
}

void optimize_at_step_size(
    chess::Engine &engine, const std::vector<CachedPosition> &positions, Settings &settings,
    const std::vector<Parameter> &parameters, double &best_loss, int step, Progress &progress,
    std::size_t step_index, std::size_t step_count
) {
    bool improved;
    std::size_t pass = 0;
    do {
        ++pass;
        progress.reset(parameters.size(), step, pass, step_index, step_count, best_loss);
        improved =
            optimize_parameters(engine, positions, settings, parameters, best_loss, step, progress);
        progress.finish(best_loss);
    } while (improved);
}

void print_settings(const Settings &settings, double final_loss) {
    std::cout << "\nTuned settings:\n";
    std::cout << "int32_t king_val = " << settings.king_val << '\n';
    std::cout << "int32_t pawn_val = " << settings.pawn_val << '\n';

    for (const auto &parameter : kBaseParameters) {
        if (parameter.new_group) { std::cout << '\n'; }
        std::cout << "int32_t " << parameter.name << " = "
                  << get_parameter_value(settings, parameter) << '\n';
    }

    std::cout << '\n';
    std::cout << "std::array<int32_t, 9> rook_adj = {";
    for (std::size_t i = 0; i < settings.rook_adj.size(); ++i) {
        if (i > 0) { std::cout << ", "; }
        std::cout << settings.rook_adj[i];
    }
    std::cout << "};\n";

    std::cout << "std::array<int32_t, 9> knight_adj = {";
    for (std::size_t i = 0; i < settings.knight_adj.size(); ++i) {
        if (i > 0) { std::cout << ", "; }
        std::cout << settings.knight_adj[i];
    }
    std::cout << "};\n";

    std::cout << '\n';
    std::cout << "// Replace make_default_pst() in engine.hpp with this:\n";
    std::cout << "static constexpr PieceArray<PieceSquareTables> make_default_pst() {\n";
    std::cout << "    PieceArray<PieceSquareTables> result{};\n\n";

    for (std::size_t piece_idx = 0; piece_idx < kPieceTypes.size(); ++piece_idx) {

        const auto piece = kPieceTypes[piece_idx];
        const auto &tables = settings.pst[piece];

        for (const bool endgame : {false, true}) {
            const auto &values = endgame ? tables.endgame : tables.middlegame;

            std::cout << "    result[PieceType::" << kPieceNames[piece_idx] << "]."
                      << (endgame ? "endgame" : "middlegame") << " = convert_formatted_board({\n";
            std::cout << "        // clang-format off\n";

            for (chess::rank_t rank = 7; rank >= 0; --rank) {
                std::cout << "        /*" << rank + 1 << "*/ ";
                for (chess::file_t file = 0; file < 8; ++file) {
                    if (file > 0) { std::cout << ' '; }
                    std::cout << std::setw(4) << values[file + rank * 8];
                    if (file < 7) { std::cout << ','; }
                }

                std::cout << (rank > 0 ? ",\n" : "\n");
            }

            std::cout << "        //       A     B     C     D     E     F     G     H\n";
            std::cout << "        // clang-format on\n";
            std::cout << "    });\n";
        }

        std::cout << '\n';
    }

    std::cout << "    return result;\n";
    std::cout << "}\n";

    std::cout << "\nFinal training loss: " << final_loss << '\n';
}

int main(int argc, char **argv) {
    try {
        const std::string dataset_path = (argc > 1) ? argv[1] : "positions.tsv";

        chess::Engine engine;
        const auto positions = load_positions(dataset_path, engine);
        Settings settings{};
        const auto parameters = make_parameters();

        double best_loss = calculate_loss(engine, positions, settings);

        std::cout << "Positions: " << positions.size() << '\n';
        std::cout << std::setprecision(10);
        std::cout << "Initial loss: " << best_loss << '\n';

        constexpr std::array<int, 6> kStepSizes{50, 25, 10, 5, 2, 1};

        std::size_t completed_before = 0;

        Progress progress;
        progress.start_run();

        for (std::size_t i = 0; i < kStepSizes.size(); ++i) {
            const int step = kStepSizes[i];

            std::cout << "\nOptimizing with step size " << step << "...\n";

            optimize_at_step_size(
                engine,
                positions,
                settings,
                parameters,
                best_loss,
                step,
                progress,
                i + 1,
                kStepSizes.size()
            );

            std::cout << "Finished step size " << step << ". Best loss: " << std::setprecision(10)
                      << best_loss << "\n";
        }

        print_settings(settings, best_loss);
    } catch (const std::exception &error) {
        std::cerr << "\nError: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
