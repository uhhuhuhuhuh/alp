#include "engine.hpp"
#include "fen_pos.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace {

struct Options {
    std::string input;
    std::string output = "positions.tsv";
    std::optional<int> game_limit;
    int step = 1;
    std::optional<int> min_rating;
    std::optional<int> max_rating;
    std::optional<double> min_base_time;
    std::optional<double> max_base_time;
    std::optional<std::string> time_control;
};

struct PgnGame {
    std::unordered_map<std::string, std::string> headers;
    std::string movetext;
};

struct Stats {
    std::uint64_t games_examined = 0;
    std::uint64_t games_filtered = 0;
    std::uint64_t games_processed = 0;
    std::uint64_t positions_seen = 0;
    std::uint64_t positions_written = 0;
};

constexpr bool is_white(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

std::string_view trim_view(std::string_view s) {
    while (!s.empty() && is_white(static_cast<unsigned char>(s.front()))) { s.remove_prefix(1); }
    while (!s.empty() && is_white(static_cast<unsigned char>(s.back()))) { s.remove_suffix(1); }
    return s;
}

bool is_blank(std::string_view s) {
    for (unsigned char c : s) {
        if (!is_white(c)) return false;
    }
    return true;
}

std::optional<int> parse_int(std::string_view s) {
    int value{};
    const char *begin = s.data();
    const char *end = begin + s.size();
    auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end) return std::nullopt;
    return value;
}

std::optional<double> parse_double(std::string_view s) {
    double value{};
    const char *begin = s.data();
    const char *end = begin + s.size();
    auto [ptr, ec] = std::from_chars(begin, end, value, std::chars_format::general);
    if (ec != std::errc{} || ptr != end || !std::isfinite(value)) return std::nullopt;
    return value;
}

std::optional<int> parse_rating(const PgnGame &game, const char *color) {
    auto it = game.headers.find(std::string(color) + "Elo");
    if (it == game.headers.end()) return std::nullopt;
    return parse_int(it->second);
}

std::optional<double> parse_base_time(const PgnGame &game) {
    auto it = game.headers.find("TimeControl");
    if (it == game.headers.end() || it->second.empty() || it->second == "-") {
        return std::nullopt;
    }

    std::string_view first_period(it->second);
    first_period = first_period.substr(0, first_period.find(':'));

    const auto slash = first_period.rfind('/');
    if (slash != std::string_view::npos) first_period.remove_prefix(slash + 1);

    const auto plus = first_period.find('+');
    if (plus != std::string_view::npos) first_period = first_period.substr(0, plus);

    return parse_double(first_period);
}

bool passes_filters(const PgnGame &game, const Options &o) {
    const auto white = parse_rating(game, "White");
    const auto black = parse_rating(game, "Black");

    if (o.min_rating || o.max_rating) {
        if (!white || !black) return false;
        if (o.min_rating && std::min(*white, *black) < *o.min_rating) { return false; }
        if (o.max_rating && std::max(*white, *black) > *o.max_rating) { return false; }
    }

    if (o.time_control) {
        auto it = game.headers.find("TimeControl");
        if (it == game.headers.end() || it->second != *o.time_control) { return false; }
    }

    if (o.min_base_time || o.max_base_time) {
        const auto base = parse_base_time(game);
        if (!base) return false;
        if (o.min_base_time && *base < *o.min_base_time) return false;
        if (o.max_base_time && *base > *o.max_base_time) return false;
    }

    return true;
}

bool parse_header_line(std::string_view line, std::string &key, std::string &value) {
    const std::string_view s = trim_view(line);
    if (s.size() < 4 || s.front() != '[' || s.back() != ']') return false;

    std::size_t p = 1;
    while (p < s.size() && is_white(static_cast<unsigned char>(s[p]))) { p++; }

    const std::size_t key_begin = p;
    while (p < s.size() && !is_white(static_cast<unsigned char>(s[p]))) { p++; }
    if (p == key_begin) return false;

    key.assign(s.data() + key_begin, p - key_begin);

    while (p < s.size() && is_white(static_cast<unsigned char>(s[p]))) { p++; }
    if (p >= s.size() || s[p] != '"') return false;
    p++;

    value.clear();

    while (p < s.size()) {
        if (s[p] == '\\' && p + 1 < s.size()) {
            value.push_back(s[p + 1]);
            p += 2;
        } else if (s[p] == '"') {
            return true;
        } else {
            value.push_back(s[p++]);
        }
    }

    return false;
}

enum class TokenResult { continue_parsing, end_movetext, stop };

template <typename Callback>
TokenResult append_movetext_token(std::string_view token, Callback &&callback) {
    if (token.empty()) { return TokenResult::continue_parsing; }

    // Game result.
    if (token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*") {
        return TokenResult::end_movetext;
    }

    // Remove move-number prefixes:
    // 1.e4
    // 1...e5
    if (token.front() >= '0' && token.front() <= '9') {
        std::size_t pos = 0;
        while (pos < token.size() && token[pos] >= '0' && token[pos] <= '9') { ++pos; }
        const std::size_t digits_end = pos;
        while (pos < token.size() && token[pos] == '.') { ++pos; }
        if (pos > digits_end) { token.remove_prefix(pos); }
    }

    if (token.empty() || token.front() == '$') { return TokenResult::continue_parsing; }

    if (token == "!" || token == "?" || token == "!!" || token == "??" || token == "!?" ||
        token == "?!") {
        return TokenResult::continue_parsing;
    }

    if (token == "e.p." || token == "ep") { return TokenResult::continue_parsing; }

    return callback(token) ? TokenResult::continue_parsing : TokenResult::stop;
}

template <typename Callback>
bool for_each_movetext_token(std::string_view text, Callback &&callback) {
    std::size_t token_begin = std::string_view::npos;

    unsigned variation_depth = 0;

    bool brace_comment = false;
    bool line_comment = false;

    bool keep_reading = true;
    bool callback_stopped = false;

    auto flush_token = [&](std::size_t end) -> bool {
        if (token_begin == std::string_view::npos) { return true; }

        const auto token = text.substr(token_begin, end - token_begin);

        token_begin = std::string_view::npos;

        const TokenResult result = append_movetext_token(token, callback);
        switch (result) {
        case TokenResult::continue_parsing: return true;
        case TokenResult::end_movetext:     keep_reading = false; return true;
        case TokenResult::stop:
            callback_stopped = true;
            keep_reading = false;
            return false;
        }

        return true;
    };

    for (std::size_t i = 0; i < text.size() && keep_reading; ++i) {
        const char c = text[i];

        if (line_comment) {
            if (c == '\n') { line_comment = false; }
            continue;
        }

        if (brace_comment) {
            if (c == '}') { brace_comment = false; }
            continue;
        }

        if (variation_depth != 0) {
            if (c == '(') {
                variation_depth++;
            } else if (c == ')') {
                variation_depth--;
            }
            continue;
        }

        if (c == ';' || c == '{' || c == '(') {
            if (!flush_token(i)) { break; }
            if (!keep_reading) { break; }
            if (c == ';') {
                line_comment = true;
            } else if (c == '{') {
                brace_comment = true;
            } else {
                variation_depth = 1;
            }
            continue;
        }

        if (is_white(c)) {
            if (!flush_token(i)) { break; }
            continue;
        }

        if (token_begin == std::string_view::npos) { token_begin = i; }
    }

    if (keep_reading && token_begin != std::string_view::npos) { flush_token(text.size()); }

    return !callback_stopped;
}

template <typename callback_fn> void read_pgn(std::istream &in, callback_fn &&callback) {
    PgnGame current;

    current.movetext.reserve(16 * 1024);

    std::string line;

    bool have_content = false;
    auto finish = [&]() -> bool {
        if (!have_content) { return true; }
        const bool keep_reading = callback(std::move(current));
        current = PgnGame{};
        current.movetext.reserve(16 * 1024);
        have_content = false;
        return keep_reading;
    };

    while (std::getline(in, line)) {
        std::string key, value;
        if (parse_header_line(line, key, value)) {
            if (!current.headers.empty() && !current.movetext.empty()) {
                if (!finish()) { return; }
            }

            current.headers[key] = value;
            have_content = true;
        } else if (!is_blank(line)) {
            current.movetext.append(line);
            current.movetext.push_back('\n');

            have_content = true;
        }
    }

    finish();
}

chess::PieceType san_piece_type(char c) {
    switch (c) {
    case 'N': return chess::PieceType::knight;
    case 'B': return chess::PieceType::bishop;
    case 'R': return chess::PieceType::rook;
    case 'Q': return chess::PieceType::queen;
    case 'K': return chess::PieceType::king;
    default:  return chess::PieceType::none;
    }
}

struct ParsedSan {
    chess::PieceType piece = chess::PieceType::pawn;
    chess::PieceType promotion = chess::PieceType::none;

    int destination = -1;
    int source_file = -1;
    int source_rank = -1;

    bool capture = false;

    bool castle_king_side = false;
    bool castle_queen_side = false;
};

bool parse_san(std::string_view token, ParsedSan &out) {
    // Remove trailing check, mate, and annotation markers.
    while (!token.empty()) {
        const char c = token.back();
        if (c == '+' || c == '#' || c == '!' || c == '?') token.remove_suffix(1);
        else
            break;
    }

    // Some sources append the optional en-passant marker.
    if (token.size() >= 4 && token.substr(token.size() - 4) == "e.p.") {
        token.remove_suffix(4);
        while (!token.empty() && is_white(static_cast<unsigned char>(token.back()))) {
            token.remove_suffix(1);
        }
    }

    // Castling: accept both letter-O and zero notation.
    if (token == "O-O" || token == "0-0") {
        out.castle_king_side = true;
        return true;
    }

    if (token == "O-O-O" || token == "0-0-0") {
        out.castle_queen_side = true;
        return true;
    }

    if (token.size() < 2) return false;

    // Promotion suffix: =Q, =R, =B, or =N.
    if (token.size() >= 2 && token[token.size() - 2] == '=') {
        out.promotion = san_piece_type(token.back());
        if (out.promotion != chess::PieceType::queen && out.promotion != chess::PieceType::rook &&
            out.promotion != chess::PieceType::bishop &&
            out.promotion != chess::PieceType::knight) {
            return false;
        }

        token.remove_suffix(2);
    }

    // The final two characters identify the destination square.
    const char file = token[token.size() - 2];
    const char rank = token[token.size() - 1];

    if (file < 'a' || file > 'h' || rank < '1' || rank > '8') { return false; }

    out.destination = (rank - '1') * 8 + (file - 'a');

    token.remove_suffix(2);

    // A leading piece letter distinguishes non-pawn SAN.
    if (!token.empty()) {
        const chess::PieceType type = san_piece_type(token.front());

        if (type != chess::PieceType::none) {
            out.piece = type;
            token.remove_prefix(1);
        }
    }

    // Remaining characters are disambiguation and capture marker.
    for (char c : token) {
        if (c == 'x') {
            if (out.capture) return false;

            out.capture = true;
        } else if (c >= 'a' && c <= 'h') {
            if (out.source_file != -1) return false;

            out.source_file = c - 'a';
        } else if (c >= '1' && c <= '8') {
            if (out.source_rank != -1) return false;

            out.source_rank = c - '1';
        } else {
            return false;
        }
    }

    if (out.piece == chess::PieceType::pawn) {
        // Pawn captures must specify their originating file.
        if (out.capture != (out.source_file != -1) || out.source_rank != -1) { return false; }
    }

    if (out.promotion != chess::PieceType::none && out.piece != chess::PieceType::pawn) {
        return false;
    }

    return true;
}

bool san_move_is_capture(const std::array<chess::Piece, 64> &board, const chess::Move &move) {
    const int from = static_cast<int>(move.from);
    const int to = static_cast<int>(move.to);

    if (from < 0 || from >= 64 || to < 0 || to >= 64) { return false; }

    if (board[to].type != chess::PieceType::none) return true;

    return board[from].type == chess::PieceType::pawn && (from % 8) != (to % 8);
}

bool san_to_move(chess::Engine &engine, std::string_view token, chess::Move &result) {
    ParsedSan san;

    if (!parse_san(token, san)) return false;

    const auto &board = engine.position().board;

    const auto &legal_moves = engine.gen_legal_moves();

    bool found = false;

    for (const chess::Move &move : legal_moves) {
        const int from = static_cast<int>(move.from);
        const int to = static_cast<int>(move.to);

        if (from < 0 || from >= 64 || to < 0 || to >= 64) { continue; }

        const chess::Piece moving = board[from];

        if (san.castle_king_side || san.castle_queen_side) {
            if (moving.type != chess::PieceType::king || (to - from != 2 && from - to != 2)) {
                continue;
            }

            const bool king_side = to > from;

            if (king_side != san.castle_king_side) continue;
        } else {
            if (to != san.destination || moving.type != san.piece) { continue; }
            if (san.source_file != -1 && from % 8 != san.source_file) { continue; }
            if (san.source_rank != -1 && from / 8 != san.source_rank) { continue; }
            if (move.promotion_type != san.promotion) continue;

            if (san_move_is_capture(board, move) != san.capture) continue;
        }

        if (found) return false;

        result = move;
        found = true;
    }

    return found;
}

bool parse_game_position(chess::Engine &engine, const PgnGame &game) {
    const auto fen = game.headers.find("FEN");
    if (fen != game.headers.end()) {
        return engine.parse_fen(fen->second) == chess::Position::FenParseErr::ok;
    }
    return engine.parse_fen(chess::fen::starting_pos) == chess::Position::FenParseErr::ok;
}

void process_game(
    chess::Engine &engine, const PgnGame &game, const Options &options, std::ofstream &output,
    Stats &stats
) {
    const auto result_it = game.headers.find("Result");

    const std::string result = result_it == game.headers.end() ? "*" : result_it->second;

    double white_score;
    if (result == "1-0") {
        white_score = 1.0;
    } else if (result == "1/2-1/2") {
        white_score = 0.5;
    } else if (result == "0-1") {
        white_score = 0.0;
    } else {
        stats.games_filtered++;
        return;
    }

    // Filter before tokenizing movetext.
    if (!passes_filters(game, options)) {
        stats.games_filtered++;
        return;
    }

    if (!parse_game_position(engine, game)) {
        stats.games_filtered++;
        std::cerr << "Warning: skipping game with invalid starting FEN.\n";
        return;
    }

    std::uint64_t ply = 0;

    const bool success = for_each_movetext_token(game.movetext, [&](std::string_view san) -> bool {
        chess::Move move;
        if (!san_to_move(engine, san, move)) {
            std::cerr << "Warning: skipping game; could not resolve SAN move: " << san << '\n';

            return false;
        }

        engine.do_move(move);
        stats.positions_seen++;
        ply++;
        if (ply % options.step == 0) {
            output << engine.position().format_as_fen() << '\t' << white_score << '\n';
            stats.positions_written++;
        }
        return true;
    });
    if (!success) {
        stats.games_filtered++;
        return;
    }
    stats.games_processed++;
}

void print_usage(const char *program) {
    std::cerr << "Usage: " << program << " INPUT [options]\n"
              << "  -o, --output PATH       Output TSV (default: positions.tsv)\n"
              << "  --game-limit N          Maximum accepted games to process\n"
              << "  --step N                Save every Nth position (default: 1)\n"
              << "  --min-rating N          Minimum rating required for both players\n"
              << "  --max-rating N          Maximum rating allowed for both players\n"
              << "  --min-base-time SEC     Minimum initial clock time in seconds\n"
              << "  --max-base-time SEC     Maximum initial clock time in seconds\n"
              << "  --time-control VALUE    Exact TimeControl header match\n";
}

bool parse_options(int argc, char **argv, Options &o) {
    if (argc < 2) return false;

    o.input = argv[1];

    auto need_value = [&](int &i, const char *option) -> std::optional<std::string> {
        if (i + 1 >= argc) {
            std::cerr << "Missing value for " << option << '\n';
            return std::nullopt;
        }
        return std::string(argv[++i]);
    };

    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") return false;

        if (arg == "-o" || arg == "--output") {
            auto v = need_value(i, arg.c_str());
            if (!v) return false;
            o.output = *v;
        } else if (arg == "--time-control") {
            auto v = need_value(i, arg.c_str());
            if (!v) return false;
            o.time_control = *v;
        } else if (
            arg == "--game-limit" || arg == "--step" || arg == "--min-rating" ||
            arg == "--max-rating"
        ) {
            auto v = need_value(i, arg.c_str());
            if (!v) return false;
            auto n = parse_int(*v);
            if (!n) {
                std::cerr << "Invalid integer for " << arg << '\n';
                return false;
            }
            if (arg == "--game-limit") o.game_limit = *n;
            else if (arg == "--step")
                o.step = *n;
            else if (arg == "--min-rating")
                o.min_rating = *n;
            else
                o.max_rating = *n;
        } else if (arg == "--min-base-time" || arg == "--max-base-time") {
            auto v = need_value(i, arg.c_str());
            if (!v) return false;
            auto n = parse_double(*v);
            if (!n) {
                std::cerr << "Invalid number for " << arg << '\n';
                return false;
            }
            if (arg == "--min-base-time") o.min_base_time = *n;
            else
                o.max_base_time = *n;
        } else {
            std::cerr << "Unknown option: " << arg << '\n';
            return false;
        }
    }

    if (o.step < 1 || (o.game_limit && *o.game_limit < 1)) {
        std::cerr << "Step and game limit must be at least 1.\n";
        return false;
    }

    return true;
}

void print_progress(std::uint64_t bytes_read, std::uint64_t total_bytes, const Stats &stats) {
    constexpr int bar_width = 40;
    double progress =
        total_bytes > 0 ? std::min(1.0, static_cast<double>(bytes_read) / total_bytes) : 0.0;
    int filled = static_cast<int>(progress * bar_width);
    std::cerr << '\r' << '[';
    for (int i = 0; i < bar_width; ++i) std::cerr << (i < filled ? '#' : '-');
    std::cerr << "] " << std::fixed << std::setprecision(1) << (progress * 100.0) << "% "
              << std::defaultfloat << "| Games: " << stats.games_examined
              << " | Games Processed: " << stats.games_processed
              << " | Positions Written: " << stats.positions_written << std::flush;
}

} // namespace

int main(int argc, char **argv) {
    Options options;
    if (!parse_options(argc, argv, options)) {
        print_usage(argv[0]);
        return 1;
    }

    std::ifstream input(options.input, std::ios::binary);
    if (!input) {
        std::cerr << "Could not open input PGN: " << options.input << '\n';
        return 1;
    }

    std::ofstream output(options.output);
    if (!output) {
        std::cerr << "Could not open output TSV: " << options.output << '\n';
        return 1;
    }

    const std::uint64_t total_bytes = std::filesystem::file_size(options.input);
    Stats stats;
    auto last_update = std::chrono::steady_clock::now();
    chess::Engine engine;
    read_pgn(input, [&](PgnGame &&game) {
        if (options.game_limit &&
            stats.games_processed >= static_cast<std::uint64_t>(*options.game_limit)) {
            return false;
        }
        stats.games_examined++;

        process_game(engine, game, options, output, stats);

        const auto now = std::chrono::steady_clock::now();
        if (now - last_update >= std::chrono::milliseconds(250)) {
            const auto position = input.tellg();
            const std::uint64_t bytes_read =
                position >= 0 ? static_cast<std::uint64_t>(position) : total_bytes;
            print_progress(bytes_read, total_bytes, stats);
            last_update = now;
        }

        return !options.game_limit ||
               stats.games_processed < static_cast<std::uint64_t>(*options.game_limit);
    });

    const auto position = input.tellg();

    const std::uint64_t bytes_read =
        position >= 0 ? static_cast<std::uint64_t>(position) : total_bytes;
    print_progress(bytes_read, total_bytes, stats);

    std::cerr << '\n';
    std::cout << "Games examined: " << stats.games_examined << '\n'
              << "Games filtered out: " << stats.games_filtered << '\n'
              << "Games processed: " << stats.games_processed << '\n'
              << "Positions examined: " << stats.positions_seen << '\n'
              << "Positions written: " << stats.positions_written << '\n'
              << "Output file: " << options.output << '\n';

    return 0;
}
