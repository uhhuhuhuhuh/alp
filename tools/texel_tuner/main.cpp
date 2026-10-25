#include "engine.hpp"
#include "position.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

struct Sample {
    std::string fen;
    double result; // 1.0 = White wins, 0.5 = draw, 0.0 = Black wins
};

using Settings = chess::Engine::EvalSettings;

constexpr double kScalingFactor = 1.0;
constexpr double kImprovementThreshold = 1e-12;

// Load positions from a TSV file containing:
// FEN<TAB>result
std::vector<Sample> load_samples(const std::string &path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Could not open " + path);
    }

    std::vector<Sample> samples;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto tab_position = line.rfind('\t');
        if (tab_position == std::string::npos) {
            throw std::runtime_error("Expected FEN<TAB>result");
        }
        const std::string fen = line.substr(0, tab_position);
        const double result = std::stod(line.substr(tab_position + 1));
        if (result < 0.0 || result > 1.0) {
            throw std::runtime_error("Result must be between 0 and 1");
        }
        samples.push_back({fen, result});
    }

    if (samples.empty()) {
        throw std::runtime_error("Dataset is empty");
    }

    return samples;
}

double predict_white_score(double evaluation) {
    const double exponent = -kScalingFactor * std::log(10.0) * evaluation / 400.0;
    if (exponent >= 0.0) {
        const double exp_negative = std::exp(-exponent);
        return exp_negative / (1.0 + exp_negative);
    }

    return 1.0 / (1.0 + std::exp(exponent));
}

double calculate_loss(
    chess::Engine &engine, const std::vector<Sample> &samples, const Settings &settings
) {
    double total_squared_error = 0.0;

    for (const auto &sample : samples) {
        const auto parse_error = engine.parse_fen(sample.fen);
        if (parse_error != chess::Position::FenParseErr::ok) {
            exit(1);
        }

        double evaluation = engine.evaluate(settings);
        if (engine.pos.st->active_color == chess::Color::black) {
            evaluation = -evaluation;
        }
        const double prediction = predict_white_score(evaluation);

        const double difference = prediction - sample.result;
        total_squared_error += difference * difference;
    }

    return total_squared_error / static_cast<double>(samples.size());
}

struct Parameter {
    const char *name;
    int32_t Settings::*member;
    int32_t minimum;
    int32_t maximum;
    std::optional<std::size_t> rook_adj_idx{};
    std::optional<std::size_t> knight_adj_idx{};
};

constexpr std::array<Parameter, 27> kParameters{
    {
     {"queen_val", &Settings::queen_val, 700, 1200},
     {"rook_val", &Settings::rook_val, 350, 700},
     {"bishop_val", &Settings::bishop_val, 150, 450},
     {"knight_val", &Settings::knight_val, 150, 450},
     {"blocked_penalty", &Settings::blocked_penalty, 0, 150},
     {"doubled_penalty", &Settings::doubled_penalty, 0, 150},
     {"isolated_penalty", &Settings::isolated_penalty, 0, 150},
     {"backward_penalty", &Settings::backward_penalty, 0, 150},
     {"passed_val", &Settings::passed_val, 0, 150},
     {"rook_adj[0]", nullptr, -50, 50, 0},
     {"rook_adj[1]", nullptr, -50, 50, 1},
     {"rook_adj[2]", nullptr, -50, 50, 2},
     {"rook_adj[3]", nullptr, -50, 50, 3},
     {"rook_adj[4]", nullptr, -50, 50, 4},
     {"rook_adj[5]", nullptr, -50, 50, 5},
     {"rook_adj[6]", nullptr, -50, 50, 6},
     {"rook_adj[7]", nullptr, -50, 50, 7},
     {"rook_adj[8]", nullptr, -50, 50, 8},
     {"knight_adj[0]", nullptr, -50, 50, {}, 0},
     {"knight_adj[1]", nullptr, -50, 50, {}, 1},
     {"knight_adj[2]", nullptr, -50, 50, {}, 2},
     {"knight_adj[3]", nullptr, -50, 50, {}, 3},
     {"knight_adj[4]", nullptr, -50, 50, {}, 4},
     {"knight_adj[5]", nullptr, -50, 50, {}, 5},
     {"knight_adj[6]", nullptr, -50, 50, {}, 6},
     {"knight_adj[7]", nullptr, -50, 50, {}, 7},
     {"knight_adj[8]", nullptr, -50, 50, {}, 8},
     }
};

bool optimize_parameters(
    chess::Engine &engine, const std::vector<Sample> &samples, Settings &settings,
    double &best_loss, int step
) {
    bool improved = false;

    for (const auto &parameter : kParameters) {
        int32_t &current_value =
            parameter.rook_adj_idx.has_value()     ? settings.rook_adj[*parameter.rook_adj_idx]
            : parameter.knight_adj_idx.has_value() ? settings.knight_adj[*parameter.knight_adj_idx]
                                                   : settings.*(parameter.member);

        const int32_t original_value = current_value;
        int32_t best_value = original_value;

        for (const int direction : {-1, 1}) {
            const int candidate = original_value + direction * step;
            if (candidate < parameter.minimum || candidate > parameter.maximum) {
                continue;
            }

            current_value = candidate;

            const double candidate_loss = calculate_loss(engine, samples, settings);
            if (candidate_loss < best_loss - kImprovementThreshold) {
                best_loss = candidate_loss;
                best_value = candidate;
            }
        }

        current_value = best_value;

        if (best_value != original_value) {
            improved = true;
            std::cout << parameter.name << " = " << best_value << ", loss = " << best_loss << '\n';
        }
    }

    return improved;
}

void optimize_at_step_size(
    chess::Engine &engine, const std::vector<Sample> &samples, Settings &settings,
    double &best_loss, int step
) {
    bool improved;
    do {
        improved = optimize_parameters(engine, samples, settings, best_loss, step);
    } while (improved);
}

void print_settings(const Settings &settings, double final_loss) {
    std::cout << "\nTuned settings:\n";
    std::cout << "king_val = " << settings.king_val << '\n';
    std::cout << "pawn_val = " << settings.pawn_val << '\n';

    for (const auto &parameter : kParameters) {
        const int32_t value = parameter.rook_adj_idx ? settings.rook_adj[*parameter.rook_adj_idx]
                              : parameter.knight_adj_idx
                                  ? settings.knight_adj[*parameter.knight_adj_idx]
                                  : settings.*(parameter.member);
        std::cout << parameter.name << " = " << value << '\n';
    }

    std::cout << "Final training loss: " << final_loss << '\n';
}

int main(int argc, char **argv) {
    try {
        const std::string dataset_path = (argc > 1) ? argv[1] : "positions.tsv";
        const auto samples = load_samples(dataset_path);

        chess::Engine engine;
        Settings settings{};
        double best_loss = calculate_loss(engine, samples, settings);

        std::cout << "Positions: " << samples.size() << '\n';
        std::cout << std::setprecision(8);
        std::cout << "Initial loss: " << best_loss << '\n';

        constexpr std::array<int, 6> kStepSizes{50, 25, 10, 5, 2, 1};
        for (const int step : kStepSizes) {
            optimize_at_step_size(engine, samples, settings, best_loss, step);
        }

        print_settings(settings, best_loss);
    } catch (const std::exception &error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
