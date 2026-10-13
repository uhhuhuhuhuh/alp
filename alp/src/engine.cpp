#include "engine.hpp"
#include "genmoves.hpp"
#include "position.hpp"
#include "tranpostable.hpp"

chess::Engine::Engine() : st_stack{}, pos{}, moves{}, tt{} {}
chess::Engine::Engine(std::string_view fen, Position::FenParseErr &out_err)
    : st_stack{}, pos{}, moves{}, tt{} {
    parse_fen(fen);
}

chess::Position::FenParseErr chess::Engine::parse_fen(std::string_view fen) {
    auto err = pos.parse_fen(fen, st_stack.emplace());
    if (err != Position::FenParseErr::ok) {
        st_stack.pop();
    }
    return err;
}

chess::Position &chess::Engine::position() { return pos; }
const chess::Position &chess::Engine::position() const { return pos; }

const chess::MoveStack &chess::Engine::gen_legal_moves() {
    return moves = gen_moves<GenType::legal>(pos);
}

void chess::Engine::do_move(const Move &move) { pos.do_move(move, st_stack.emplace()); }

void chess::Engine::undo_move(const Move &move) {
    pos.undo_move(move);
    st_stack.pop();
}

namespace chess {
static uint8_t doubled(const Position &pos, Color color) {
    bb::U64 pawns = pos.bb.get(PieceType::pawn)[color];
    int8_t doubled = 0;
    for (uint8_t file = 0; file < 8; ++file) {
        if (popcount(pawns & bb::files[file]) > 1) {
            ++doubled;
        }
    }
    return doubled;
}

static inline uint8_t blocked(const Position &pos, Color color) {
    if (color == Color::white) {
        return popcount(pos.bb.get(PieceType::pawn)[color] & pos.bb.occupancy() >> 8);
    }
    return popcount(pos.bb.get(PieceType::pawn)[color] & pos.bb.occupancy() << 8);
}

static inline uint8_t isolated(const Position &pos, Color color) {
    bb::U64 pawns = pos.bb.get(PieceType::pawn)[color];
    uint8_t count = 0;

    for (int f = 0; f < 8; ++f) {
        bb::U64 file = bb::files[f];

        if (!(pawns & file)) {
            continue;
        }

        bb::U64 adjacent = 0;
        if (f > 0)
            adjacent |= bb::files[f - 1];
        if (f < 7)
            adjacent |= bb::files[f + 1];

        if (!(pawns & adjacent)) {
            count += popcount(pawns & file);
        }
    }

    return count;
}

inline bb::U64 get_pawn_attacks(bb::U64 pawns, Color color) {
    bb::U64 attacks = 0ULL;
    for (sqr_t sqr = 0; sqr < 64; ++sqr) {
        if (pawns & bb::bb(sqr)) {
            attacks |= bb::pawn_attacks[color][sqr];
        }
    }
    return attacks;
}

inline uint8_t backward(const Position &pos, Color color) {
    const bb::U64 own_pawns = pos.bb.get(PieceType::pawn)[color];
    const bb::U64 enemy_pawns = pos.bb.get(PieceType::pawn)[!color];

    const bb::U64 empty = ~pos.bb.occupancy();

    const bb::U64 enemy_attacks = get_pawn_attacks(enemy_pawns, !color);
    const bb::U64 own_attacks = get_pawn_attacks(own_pawns, color);

    bb::U64 candidates = 0ULL;
    bb::U64 pawns = own_pawns;

    while (pawns) {
        const sqr_t sqr = lsb(pawns);
        pawns &= pawns - 1;

        if (color == Color::white ? sqr >= 56 : sqr < 8)
            continue;

        const sqr_t stop = color == Color::white ? sqr + 8 : sqr - 8;

        const bb::U64 stop_bb = bb::bb(stop);

        if (!(empty & stop_bb))
            continue;

        if (!(enemy_attacks & stop_bb))
            continue;

        if (own_attacks & stop_bb)
            continue;

        candidates |= bb::bb(sqr);
    }

    return popcount(candidates);
}

constexpr int MAX_PHASE = 24;

template <typename T>
constexpr std::array<T, 64> convert_formatted_board(const std::array<T, 64> &board) {
    std::array<T, 64> arr{};
    for (std::size_t i = 0; i < 64; i++) {
        file_t file = i % 8;
        rank_t rank = 7 - (i / 8);
        arr[file + rank * 8] = board[i];
    }
    return arr;
}

enum class GamePhase { middlegame, endgame };

template <typename T> using PhasePST = std::array<T, 64>;

template <typename T> struct PieceSquareTables {
    PhasePST<T> middlegame;
    PhasePST<T> endgame;
};

constexpr auto pst = [] {
    PieceArray<PieceSquareTables<int16_t>> result{};

    result[PieceType::pawn].middlegame = convert_formatted_board<int16_t>({
        // clang-format off
        /*8*/   0,   0,   0,   0,   0,   0,   0,   0,
        /*7*/  10,  10,  20,  30,  30,  20,  10,  10,
        /*6*/   5,   5,  10,  25,  25,  10,   5,   5,
        /*5*/   0,   0,   0,  20,  20,   0,   0,   0,
        /*4*/   5,  -5, -10,   0,   0, -10,  -5,   5,
        /*3*/   5,  10,  10, -20, -20,  10,  10,   5,
        /*2*/   0,   0,   0,   0,   0,   0,   0,   0,
        /*1*/   0,   0,   0,   0,   0,   0,   0,   0
        //      A    B    C    D    F    E    G    H
        // clang-format on
    });
    result[PieceType::pawn].endgame = convert_formatted_board<int16_t>({
        // clang-format off
        /*8*/   0,   0,   0,   0,   0,   0,   0,   0,
        /*7*/  80,  80,  80,  80,  80,  80,  80,  80,
        /*6*/  50,  50,  50,  50,  50,  50,  50,  50,
        /*5*/  30,  30,  30,  30,  30,  30,  30,  30,
        /*4*/  15,  15,  15,  20,  20,  15,  15,  15,
        /*3*/   5,   5,  10,  15,  15,  10,   5,   5,
        /*2*/   0,   0,   0,   0,   0,   0,   0,   0,
        /*1*/   0,   0,   0,   0,   0,   0,   0,   0
        //      A    B    C    D    F    E    G    H
        // clang-format on
    });

    result[PieceType::knight].middlegame = convert_formatted_board<int16_t>({
        // clang-format off
        /*8*/ -50, -40, -30, -30, -30, -30, -40, -50,
        /*7*/ -40, -20,   0,   5,   5,   0, -20, -40,
        /*6*/ -30,   5,  10,  15,  15,  10,   5, -30,
        /*5*/ -30,   0,  15,  20,  20,  15,   0, -30,
        /*4*/ -30,   5,  15,  20,  20,  15,   5, -30,
        /*3*/ -30,   0,  10,  15,  15,  10,   0, -30,
        /*2*/ -40, -20,   0,   0,   0,   0, -20, -40,
        /*1*/ -50, -40, -30, -30, -30, -30, -40, -50
        //      A    B    C    D    F    E    G    H
        // clang-format on
    });
    result[PieceType::knight].endgame = convert_formatted_board<int16_t>({
        // clang-format off
        /*8*/ -50, -40, -30, -30, -30, -30, -40, -50,
        /*7*/ -40, -20,   0,   5,   5,   0, -20, -40,
        /*6*/ -30,   5,  15,  20,  20,  15,   5, -30,
        /*5*/ -30,   5,  20,  25,  25,  20,   5, -30,
        /*4*/ -30,   5,  20,  25,  25,  20,   5, -30,
        /*3*/ -30,   5,  15,  20,  20,  15,   5, -30,
        /*2*/ -40, -20,   0,   5,   5,   0, -20, -40,
        /*1*/ -50, -40, -30, -30, -30, -30, -40, -50
        //      A    B    C    D    F    E    G    H
        // clang-format on
    });

    result[PieceType::bishop].middlegame = convert_formatted_board<int16_t>({
        // clang-format off
        /*8*/ -20, -10, -10, -10, -10, -10, -10, -20,
        /*7*/ -10,   5,   0,   0,   0,   0,   5, -10,
        /*6*/ -10,  10,  10,  10,  10,  10,  10, -10,
        /*5*/ -10,   0,  10,  10,  10,  10,   0, -10,
        /*4*/ -10,   5,   5,  10,  10,   5,   5, -10,
        /*3*/ -10,   0,   5,  10,  10,   5,   0, -10,
        /*2*/ -10,   0,   0,   0,   0,   0,   0, -10,
        /*1*/ -20, -10, -10, -10, -10, -10, -10, -20
        //      A    B    C    D    F    E    G    H
        // clang-format on
    });
    result[PieceType::bishop].endgame = convert_formatted_board<int16_t>({
        // clang-format off
        /*8*/ -20, -10, -10, -10, -10, -10, -10, -20,
        /*7*/ -10,   5,   5,   5,   5,   5,   5, -10,
        /*6*/ -10,  10,  15,  15,  15,  15,  10, -10,
        /*5*/ -10,   5,  15,  20,  20,  15,   5, -10,
        /*4*/ -10,   5,  15,  20,  20,  15,   5, -10,
        /*3*/ -10,   5,  10,  15,  15,  10,   5, -10,
        /*2*/ -10,   5,   5,   5,   5,   5,   5, -10,
        /*1*/ -20, -10, -10, -10, -10, -10, -10, -20
        //      A    B    C    D    F    E    G    H
        // clang-format on
    });

    result[PieceType::rook].middlegame = convert_formatted_board<int16_t>({
        // clang-format off
        /*8*/   0,   0,   5,  10,  10,   5,   0,   0,
        /*7*/   5,  10,  10,  10,  10,  10,  10,   5,
        /*6*/  -5,   0,   0,   0,   0,   0,   0,  -5,
        /*5*/  -5,   0,   0,   0,   0,   0,   0,  -5,
        /*4*/  -5,   0,   0,   0,   0,   0,   0,  -5,
        /*3*/  -5,   0,   0,   0,   0,   0,   0,  -5,
        /*2*/  -5,   0,   0,   0,   0,   0,   0,  -5,
        /*1*/   0,   0,   5,  10,  10,   5,   0,   0
        //      A    B    C    D    F    E    G    H
        // clang-format on
    });
    result[PieceType::rook].endgame = convert_formatted_board<int16_t>({
        // clang-format off
        /*8*/   0,   0,   5,  10,  10,   5,   0,   0,
        /*7*/   5,  10,  15,  15,  15,  15,  10,   5,
        /*6*/  -5,   0,   5,   5,   5,   5,   0,  -5,
        /*5*/  -5,   0,   5,   5,   5,   5,   0,  -5,
        /*4*/  -5,   0,   5,   5,   5,   5,   0,  -5,
        /*3*/  -5,   0,   5,   5,   5,   5,   0,  -5,
        /*2*/  -5,   0,   5,   5,   5,   5,   0,  -5,
        /*1*/   0,   0,   5,  10,  10,   5,   0,   0
        //      A    B    C    D    F    E    G    H
        // clang-format on
    });

    result[PieceType::queen].middlegame = convert_formatted_board<int16_t>({
        // clang-format off
        /*8*/ -20, -10, -10,  -5,  -5, -10, -10, -20,
        /*7*/ -10,   0,   5,   0,   0,   0,   0, -10,
        /*6*/ -10,   5,   5,   5,   5,   5,   0, -10,
        /*5*/  -5,   0,   5,   5,   5,   5,   0,  -5,
        /*4*/   0,   0,   5,   5,   5,   5,   0,  -5,
        /*3*/ -10,   0,   5,   5,   5,   5,   0, -10,
        /*2*/ -10,   0,   0,   0,   0,   0,   0, -10,
        /*1*/ -20, -10, -10,  -5,  -5, -10, -10, -20
        //      A    B    C    D    F    E    G    H
        // clang-format on
    });
    result[PieceType::queen].endgame = convert_formatted_board<int16_t>({
        // clang-format off
        /*8*/ -20, -10, -10,  -5,  -5, -10, -10, -20,
        /*7*/ -10,   0,   5,   5,   5,   5,   0, -10,
        /*6*/ -10,   5,  10,  10,  10,  10,   5, -10,
        /*5*/  -5,   0,  10,  10,  10,  10,   0,  -5,
        /*4*/   0,   0,  10,  10,  10,  10,   0,  -5,
        /*3*/ -10,   0,   5,   5,   5,   5,   0, -10,
        /*2*/ -10,   0,   0,   0,   0,   0,   0, -10,
        /*1*/ -20, -10, -10,  -5,  -5, -10, -10, -20
        //      A    B    C    D    F    E    G    H
        // clang-format on
    });

    result[PieceType::king].middlegame = convert_formatted_board<int16_t>({
        // clang-format off
        /*8*/ -30, -40, -40, -50, -50, -40, -40, -30,
        /*7*/ -30, -40, -40, -50, -50, -40, -40, -30,
        /*6*/ -30, -40, -40, -50, -50, -40, -40, -30,
        /*5*/ -30, -40, -40, -50, -50, -40, -40, -30,
        /*4*/ -20, -30, -30, -40, -40, -30, -30, -20,
        /*3*/ -10, -20, -20, -20, -20, -20, -20, -10,
        /*2*/  20,  20,   0,   0,   0,   0,  20,  20,
        /*1*/  20,  30,  10,   0,   0,  10,  30,  20
        //      A    B    C    D    F    E    G    H
        // clang-format on
    });
    result[PieceType::king].endgame = convert_formatted_board<int16_t>({
        // clang-format off
        /*8*/ -50, -30, -30, -30, -30, -30, -30, -50,
        /*7*/ -30, -10,   0,   5,   5,   0, -10, -30,
        /*6*/ -30,   0,  20,  30,  30,  20,   0, -30,
        /*5*/ -30,   5,  30,  40,  40,  30,   5, -30,
        /*4*/ -30,   5,  30,  40,  40,  30,   5, -30,
        /*3*/ -30,   0,  20,  30,  30,  20,   0, -30,
        /*2*/ -30, -10,   0,   5,   5,   0, -10, -30,
        /*1*/ -50, -30, -30, -30, -30, -30, -30, -50
        //      A    B    C    D    F    E    G    H
        // clang-format on
    });

    return result;
}();

static int count_pieces(const Position &pos, PieceType type) {
    return popcount(pos.bb.get(type)[Color::white]) + popcount(pos.bb.get(type)[Color::black]);
}

static int calculate_game_phase(const Position &pos) {
    int phase = 0;

    phase += 4 * count_pieces(pos, PieceType::queen);
    phase += 2 * count_pieces(pos, PieceType::rook);
    phase += count_pieces(pos, PieceType::bishop);
    phase += count_pieces(pos, PieceType::knight);

    return std::min(phase, MAX_PHASE);
}

static int32_t evaluate_piece_square(const Position &pos, Color perspective) {
    int32_t mg_score = 0;
    int32_t eg_score = 0;

    for (Color color : {Color::white, Color::black}) {
        const int sign = (color == perspective) ? 1 : -1;
        for (PieceType type :
             {PieceType::pawn,
              PieceType::knight,
              PieceType::bishop,
              PieceType::rook,
              PieceType::queen,
              PieceType::king}) {
            bb::U64 pieces = pos.bb.get(type)[color];
            while (pieces) {
                const sqr_t sq = lsb(pieces);
                pieces &= pieces - 1;
                const sqr_t relative_sq = color == Color::white ? sq : sq ^ 56;
                mg_score += sign * pst[type].middlegame[relative_sq];
                eg_score += sign * pst[type].endgame[relative_sq];
            }
        }
    }

    const int phase = calculate_game_phase(pos);
    return (mg_score * phase + eg_score * (MAX_PHASE - phase)) / MAX_PHASE;
}
} // namespace chess

int32_t chess::Engine::evaluate() const {
    Color color = pos.st->active_color;

    int32_t score = 20000 * pos.bb.count_diff(PieceType::king, color) +
                    900 * pos.bb.count_diff(PieceType::queen, color) +
                    500 * pos.bb.count_diff(PieceType::rook, color) +
                    300 * (pos.bb.count_diff(PieceType::bishop, color) +
                           pos.bb.count_diff(PieceType::knight, color)) +
                    100 * pos.bb.count_diff(PieceType::pawn, color);

    const auto &get_score = [&]<typename fn>(fn func) {
        return func(pos, color) - func(pos, !color);
    };

    score -=
        50 * (get_score(blocked) + get_score(doubled) + get_score(isolated) + get_score(backward));

    score += evaluate_piece_square(pos, color);

    return score;
}

chess::Move chess::Engine::get_tt_move(SearchContext &ctx) {
    if (auto *entry = ctx.tt.probe(pos.st->hash)) {
        if (entry->key == pos.st->hash)
            return entry->best_move;
    }

    return Move{};
}
namespace chess {
static constexpr int32_t piece_value(PieceType type) {
    switch (type) {
    case PieceType::pawn:   return 100;
    case PieceType::knight: return 300;
    case PieceType::bishop: return 300;
    case PieceType::rook:   return 500;
    case PieceType::queen:  return 900;
    case PieceType::king:   return 2000;
    default:                return 0;
    }
}
} // namespace chess
int32_t chess::Engine::capture_score(const Move &move) {
    if (move.flag != MoveFlag::capture && move.flag != MoveFlag::ep)
        return 0;

    const Color us = pos.st->active_color;

    PieceType attacker = pos.board[move.from].type;
    PieceType victim = pos.board[move.to].type;
    if (move.flag == MoveFlag::ep) {
        victim = PieceType::pawn;
    }

    return 10 * (piece_value(victim) - piece_value(attacker));
}
int32_t chess::Engine::move_score(const Move &move, const Move &pv_move, const Move &tt_move) {
    if (move == pv_move)
        return 10'000'000;

    if (move == tt_move)
        return 9'000'000;

    if (move.flag == MoveFlag::capture || move.flag == MoveFlag::ep)
        return 5'000'000 + capture_score(move);

    return 0;
}

void chess::Engine::order_moves(MoveStack &moves, const Move &pv_move, const Move &tt_move) {
    std::stable_sort(moves.begin(), moves.end(), [&](const Move &a, const Move &b) {
        return move_score(a, pv_move, tt_move) > move_score(b, pv_move, tt_move);
    });
}
void chess::Engine::update_pv(SearchContext &ctx, uint16_t ply, const Move &move) {
    if (ply >= MAX_PLY - 1)
        return;
    ctx.pv[ply][0] = move;

    int child_length = ctx.pv_length[ply + 1];

    for (int i = 0; i < child_length; ++i) {
        ctx.pv[ply][i + 1] = ctx.pv[ply + 1][i];
    }

    ctx.pv_length[ply] = child_length + 1;
}

int32_t chess::Engine::quiescence(int32_t alpha, int32_t beta, SearchContext &ctx, int qply) {
    if (Clock::now() >= ctx.deadline) {
        ctx.stopped = true;
        return 0;
    }

    const bool in_check = pos.st->checkers != 0;

    if (qply >= MAX_QPLY)
        return evaluate();

    auto moves = gen_moves<GenType::legal>(pos);

    const GameEnd end = pos.get_game_end(moves);
    if (end == GameEnd::checkmate)
        return -MATE_SCORE;
    if (end != GameEnd::proceed)
        return 0;

    if (!in_check) {
        const int32_t stand_pat = evaluate();
        if (stand_pat >= beta)
            return stand_pat;
        alpha = std::max(alpha, stand_pat);
    }

    for (const auto &move : moves) {
        const bool is_capture = move.flag == MoveFlag::capture || move.flag == MoveFlag::ep;

        if (!in_check && !is_capture && move.promotion_type == PieceType::none)
            continue;

        State st;
        pos.do_move(move, st);
        const int32_t score = -quiescence(-beta, -alpha, ctx, qply + 1);
        pos.undo_move(move);

        if (ctx.stopped)
            return 0;

        if (score >= beta)
            return score;

        alpha = std::max(alpha, score);
    }

    return alpha;
}

int32_t chess::Engine::score_to_tt(int32_t score, uint16_t ply) {
    if (score >= MATE_THRESHOLD)
        return score + ply;

    if (score <= -MATE_THRESHOLD)
        return score - ply;

    return score;
}

int32_t chess::Engine::score_from_tt(int32_t score, uint16_t ply) {
    if (score >= MATE_THRESHOLD)
        return score - ply;

    if (score <= -MATE_THRESHOLD)
        return score + ply;

    return score;
}

int32_t chess::Engine::negamax(
    uint8_t depth, uint16_t ply, int32_t alpha, int32_t beta, SearchContext &ctx
) {
    if (Clock::now() >= ctx.deadline) {
        ctx.stopped = true;
        return 0;
    }

    auto moves = chess::gen_moves<GenType::legal>(pos);
    switch (pos.get_game_end(moves)) {
    case GameEnd::proceed:   break;
    case GameEnd::checkmate: return -MATE_SCORE + ply;
    default:                 return 0;
    }

    if (depth == 0)
        return quiescence(alpha, beta, ctx);

    const int32_t original_alpha = alpha;
    const int32_t original_beta = beta;

    Move tt_move{};

    auto *entry = ctx.tt.probe(pos.st->hash);
    if (entry != nullptr && entry->key == pos.st->hash) {
        tt_move = entry->best_move;
        if (entry->depth >= depth) {
            const int32_t tt_score = score_from_tt(entry->score, ply);

            switch (entry->bound) {
            case TTBound::exact: return tt_score;
            case TTBound::lower: alpha = std::max(alpha, tt_score); break;
            case TTBound::upper: beta = std::min(beta, tt_score); break;
            }

            if (alpha >= beta)
                return tt_score;
        }
    }

    Move pv_move{};

    if (ply < ctx.prev_pv_length)
        pv_move = ctx.prev_pv[ply];

    order_moves(moves, pv_move, tt_move);

    int32_t best = -INF;
    Move best_move{};

    for (const auto &move : moves) {
        if (Clock::now() >= ctx.deadline) {
            ctx.stopped = true;
            return 0;
        }

        State st;
        pos.do_move(move, st);
        int32_t score = -negamax(depth - 1, ply + 1, -beta, -alpha, ctx);
        pos.undo_move(move);

        if (ctx.stopped)
            return 0;

        if (score > best) {
            best = score;
            best_move = move;
        }

        if (score > alpha) {
            alpha = score;
            update_pv(ctx, ply, move);
        }

        if (alpha >= beta)
            break;
    }

    TTBound bound;
    if (best <= original_alpha) {
        bound = TTBound::upper;
    } else if (best >= original_beta) {
        bound = TTBound::lower;
    } else {
        bound = TTBound::exact;
    }

    ctx.tt.set(
        pos.st->hash,
        TTentry{
            .key = pos.st->hash,
            .depth = depth,
            .score = score_to_tt(best, ply),
            .bound = bound,
            .best_move = best_move
        }
    );

    return best;
}

int32_t chess::Engine::root_search(
    chess::MoveStack &root_moves, uint8_t depth, SearchContext &ctx, Move &best_move
) {
    int32_t alpha = -INF;
    int32_t beta = INF;
    int32_t best_score = -INF;

    Move iteration_best{};
    Move pv_move{};

    if (ctx.prev_pv_length > 0)
        pv_move = ctx.prev_pv[0];

    Move tt_move = get_tt_move(ctx);

    order_moves(root_moves, pv_move, tt_move);

    for (const auto &move : root_moves) {
        if (Clock::now() >= ctx.deadline) {
            ctx.stopped = true;
            return 0;
        }

        State st;
        pos.do_move(move, st);
        int32_t score = -negamax(depth - 1, 1, -beta, -alpha, ctx);
        pos.undo_move(move);

        if (ctx.stopped)
            return 0;

        if (score > best_score) {
            best_score = score;
            iteration_best = move;

            ctx.pv[0][0] = move;
            for (int i = 0; i < ctx.pv_length[1]; ++i) {
                ctx.pv[0][i + 1] = ctx.pv[1][i];
            }
            ctx.pv_length[0] = ctx.pv_length[1] + 1;
        }
        alpha = std::max(alpha, score);
    }

    best_move = iteration_best;

    return best_score;
}

chess::Engine::SearchResult chess::Engine::search(std::chrono::milliseconds time) {
    SearchContext ctx{.deadline = Clock::now() + time, .stopped = false, .tt = tt};

    SearchResult result;

    auto root_moves = chess::gen_moves<GenType::legal>(pos);
    if (pos.get_game_end(root_moves) != GameEnd::proceed) {
        return result;
    }
    result.best_move = root_moves[0];
    for (uint16_t depth = 1;; ++depth) {
        if (Clock::now() >= ctx.deadline)
            break;

        ctx.stopped = false;
        ctx.prev_pv_length = ctx.pv_length[0];

        for (int i = 0; i < ctx.prev_pv_length; ++i) {
            ctx.prev_pv[i] = ctx.pv[0][i];
        }
        ctx.pv_length.fill(0);
        Move iteration_best = result.best_move;

        int32_t iteration_score = -INF;

        iteration_score = root_search(root_moves, depth, ctx, iteration_best);
        if (ctx.stopped)
            break;
        result.best_move = iteration_best;
        result.score = iteration_score;
        result.depth = depth;
    }

    return result;
}
