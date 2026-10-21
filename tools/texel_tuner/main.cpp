#include "engine.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

struct Sample {
    std::string fen;
    double result; // 1 = White win, 0.5 = draw, 0 = Black win
};

using Settings = chess::Engine::EvalSettings;

constexpr double K = 1.0;

// Load: FEN<TAB>result
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

        const auto tab = line.rfind('\t');
        if (tab == std::string::npos) {
            throw std::runtime_error("Expected FEN<TAB>result");
        }

        std::string fen = line.substr(0, tab);
        double result = std::stod(line.substr(tab + 1));

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

// Convert a White-perspective centipawn score to a
// predicted White score, between 0 and 1.
double predict(double score) {
    const double exponent = -K * std::log(10.0) * score / 400.0;

    // Stable sigmoid calculation.
    if (exponent >= 0.0) {
        const double e = std::exp(-exponent);
        return e / (1.0 + e);
    }

    return 1.0 / (1.0 + std::exp(exponent));
}

// Calculate the mean squared Texel loss.
double calculate_loss(
    chess::Engine &engine, const std::vector<Sample> &samples, const Settings &settings
) {
    double total = 0.0;

    for (const auto &sample : samples) {
        auto error = engine.parse_fen(sample.fen);

        // Adapt this check to your Position::FenParseErr enum.
        // Skip invalid positions or fail explicitly in production.

        const double score = engine.evaluate(settings);
        const double p = predict(score);
        const double difference = p - sample.result;

        total += difference * difference;
    }

    return total / static_cast<double>(samples.size());
}

struct Parameter {
    const char *name;
    int32_t Settings::*member;
    int32_t minimum;
    int32_t maximum;
};

int main(int argc, char **argv) {
    try {
        const std::string dataset = argc > 1 ? argv[1] : "positions.tsv";

        const auto samples = load_samples(dataset);

        // Reuse the engine. parse_fen() must fully reset the position
        // and any state that could affect static evaluation.
        chess::Engine engine;

        Settings settings{};

        const std::array<Parameter, 8> parameters{
            {{"queen_val", &Settings::queen_val, 700, 1200},
             {"rook_val", &Settings::rook_val, 350, 700},
             {"bishop_val", &Settings::bishop_val, 200, 450},
             {"knight_val", &Settings::knight_val, 200, 450},
             {"blocked_penalty", &Settings::blocked_penalty, 0, 150},
             {"doubled_penalty", &Settings::doubled_penalty, 0, 150},
             {"isolated_penalty", &Settings::isolated_penalty, 0, 150},
             {"backward_penalty", &Settings::backward_penalty, 0, 150}}
        };

        double best_loss = calculate_loss(engine, samples, settings);

        std::cout << "Positions: " << samples.size() << '\n';
        std::cout << "Initial loss: " << std::setprecision(8) << best_loss << '\n';

        for (int step : {50, 25, 10, 5, 2, 1}) {
            bool improved;

            do {
                improved = false;

                for (const auto &parameter : parameters) {
                    int32_t &current = settings.*(parameter.member);
                    const int32_t original = current;
                    int32_t best_value = original;

                    for (int direction : {-1, 1}) {
                        const int candidate = original + direction * step;

                        if (candidate < parameter.minimum || candidate > parameter.maximum) {
                            continue;
                        }

                        current = candidate;
                        const double candidate_loss = calculate_loss(engine, samples, settings);

                        if (candidate_loss < best_loss - 1e-12) {
                            best_loss = candidate_loss;
                            best_value = candidate;
                        }
                    }

                    current = best_value;

                    if (best_value != original) {
                        improved = true;
                        std::cout << parameter.name << " = " << best_value
                                  << ", loss = " << best_loss << '\n';
                    }
                }
            } while (improved);
        }

        std::cout << "\nTuned settings:\n";
        std::cout << "king_val = " << settings.king_val << '\n';
        std::cout << "pawn_val = " << settings.pawn_val << '\n';

        for (const auto &parameter : parameters) {
            std::cout << parameter.name << " = " << settings.*(parameter.member) << '\n';
        }

        std::cout << "Final training loss: " << best_loss << '\n';
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
