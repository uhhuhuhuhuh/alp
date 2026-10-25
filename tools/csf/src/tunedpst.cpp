#include "tunedpst.hpp"
#include "genmoves.hpp"
#include "misc.hpp"
#include "piece_color.hpp"
#include "position.hpp"
#include "tranpostable.hpp"

#include <limits>

chess::TunedPSTEngine::TunedPSTEngine() : st_stack{}, pos{}, moves{}, tt{} {}
chess::TunedPSTEngine::TunedPSTEngine(std::string_view fen, Position::FenParseErr &out_err)
    : st_stack{}, pos{}, moves{}, tt{} {
    out_err = parse_fen(fen);
}

chess::Position::FenParseErr chess::TunedPSTEngine::parse_fen(std::string_view fen) {
    const bool push_new = st_stack.size() == 0;
    if (push_new) {
        st_stack.emplace();
    }
    chess::State old_st = st_stack[0];
    auto err = pos.parse_fen(fen, st_stack[0]);
    if (err != Position::FenParseErr::ok) {
        st_stack[0] = old_st;
        if (push_new) {
            st_stack.clear();
        }
    } else {
        st_stack.resize(1);
    }
    return err;
}

chess::Position &chess::TunedPSTEngine::position() { return pos; }
const chess::Position &chess::TunedPSTEngine::position() const { return pos; }

const chess::MoveStack<> &chess::TunedPSTEngine::gen_legal_moves() {
    return moves = gen_moves<GenType::legal>(pos);
}

void chess::TunedPSTEngine::do_move(const Move &move) { pos.do_move(move, st_stack.emplace()); }

void chess::TunedPSTEngine::undo_move(const Move &move) {
    pos.undo_move(move);
    st_stack.pop();
}

namespace chess {
static uint8_t doubled(const Position &pos, Color color) {
    bb::U64 pawns = pos.bb.get(PieceType::pawn)[color];
    int8_t doubled = 0;
    for (uint8_t file = 0; file < 8; ++file) {
        const int count = popcount(pawns & bb::files[file]);
        if (count > 1)
            doubled += count - 1;
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

inline static bb::U64 get_pawn_attacks(bb::U64 pawns, Color color) {
    if (color == Color::white) {
        return ((pawns & ~bb::files[File::A]) << 7) | ((pawns & ~bb::files[File::H]) << 9);
    }
    return ((pawns & ~bb::files[File::A]) >> 9) | ((pawns & ~bb::files[File::H]) >> 7);
}

struct BackwardPassed {
    int16_t backward_count;
    int16_t passed_count;
};

inline static BackwardPassed get_backward_passed(const Position &pos, Color color) {
    const Color enemy = !color;

    const bb::U64 own_pawns = pos.bb.get(PieceType::pawn)[color];
    const bb::U64 enemy_pawns = pos.bb.get(PieceType::pawn)[enemy];

    const bb::U64 empty = ~pos.bb.occupancy();

    const bb::U64 enemy_attacks = get_pawn_attacks(enemy_pawns, enemy);
    const bb::U64 own_attacks = get_pawn_attacks(own_pawns, color);

    bb::U64 backward = 0ULL;
    bb::U64 passed = 0ULL;
    bb::U64 pawns = own_pawns;

    while (pawns) {
        const sqr_t sqr = lsb(pawns);
        pawns &= pawns - 1;

        if (!(bb::passed_pawn_masks[color][sqr] & enemy_pawns))
            passed |= bb::bb(sqr);

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

        backward |= bb::bb(sqr);
    }

    return BackwardPassed{
        .backward_count = static_cast<int16_t>(popcount(backward)),
        .passed_count = static_cast<int16_t>(popcount(passed))
    };
}

constexpr int MAX_PHASE = 24;

enum class GamePhase { middlegame, endgame };

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

static int32_t evaluate_piece_sqr(
    const Position &pos, Color perspective, const PieceArray<TunedPSTEngine::PieceSquareTables> &pst
) {
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

int32_t chess::TunedPSTEngine::evaluate(const EvalSettings &eval_settings) const {
    const Color color = pos.st->active_color;
    const Color enemy = !color;

    const auto &adj =
        [&](int32_t val, const std::array<int32_t, 9> &adj, PieceType type) -> int32_t {
        return ((val + adj[popcount(pos.bb.get(PieceType::pawn)[color])]) *
                popcount(pos.bb.get(type)[color])) -
               ((val + adj[popcount(pos.bb.get(PieceType::pawn)[enemy])]) *
                popcount(pos.bb.get(type)[enemy]));
    };
    const auto &popcount_diff = [&](PieceType type) -> int32_t {
        return popcount(pos.bb.get(type)[color]) - popcount(pos.bb.get(type)[enemy]);
    };

    int32_t score = eval_settings.king_val * popcount_diff(PieceType::king) +
                    eval_settings.queen_val * popcount_diff(PieceType::queen) +
                    adj(eval_settings.rook_val, eval_settings.rook_adj, PieceType::rook) +
                    eval_settings.bishop_val * popcount_diff(PieceType::bishop) +
                    adj(eval_settings.knight_val, eval_settings.knight_adj, PieceType::knight) +
                    eval_settings.pawn_val * popcount_diff(PieceType::pawn);

    const auto &get_score = [&]<typename fn>(fn func) {
        return static_cast<int16_t>(func(pos, color)) - static_cast<int16_t>(func(pos, !color));
    };

    auto our_backward_passed = get_backward_passed(pos, color);
    auto their_backward_passed = get_backward_passed(pos, enemy);
    score -= eval_settings.blocked_penalty * get_score(blocked);
    score -= eval_settings.doubled_penalty * get_score(doubled);
    score -= eval_settings.isolated_penalty * get_score(isolated);
    score -= eval_settings.backward_penalty *
             (our_backward_passed.backward_count - their_backward_passed.backward_count);

    score += eval_settings.passed_val *
             (our_backward_passed.passed_count - their_backward_passed.passed_count);

    score += evaluate_piece_sqr(pos, color, eval_settings.pst);

    return score;
}

int32_t chess::TunedPSTEngine::evaluate() const {
    static constexpr EvalSettings defaults{};
    return evaluate(defaults);
}

bool chess::TunedPSTEngine::should_stop(SearchContext &ctx, const SearchSettings &search_settings) {
    ctx.nodes++;

    assert(search_settings.time_check_interval > 0);
    if ((ctx.nodes % search_settings.time_check_interval) != 0)
        return false;

    if (Clock::now() >= ctx.deadline) {
        ctx.stopped = true;
        return true;
    }

    return false;
}

chess::Move chess::TunedPSTEngine::get_tt_move(SearchContext &ctx) {
    if (auto *entry = tt.probe(pos.st->hash)) {
        if (entry->key == pos.st->hash)
            return entry->best_move;
    }

    return Move::none();
}

int32_t chess::TunedPSTEngine::move_piece_value(PieceType type, const EvalSettings &eval_settings) {
    switch (type) {
    case PieceType::pawn:   return eval_settings.pawn_val;
    case PieceType::knight: return eval_settings.knight_val;
    case PieceType::bishop: return eval_settings.bishop_val;
    case PieceType::rook:   return eval_settings.rook_val;
    case PieceType::queen:  return eval_settings.queen_val;
    case PieceType::king:   return eval_settings.king_val;
    default:                return 0;
    }
}

namespace chess {
static constexpr std::array<PieceType, 6> SEE_ORDER = {
    PieceType::pawn,
    PieceType::knight,
    PieceType::bishop,
    PieceType::rook,
    PieceType::queen,
    PieceType::king
};

struct SeeState {
    PieceArray<ColorArray<bb::U64>> pieces{};
    bb::U64 occupied{};

    bb::U64 &get(PieceType type, Color color) { return pieces[type][color]; }
    bb::U64 get(PieceType type, Color color) const { return pieces[type][color]; }
};

static SeeState make_see_state(const Position &pos) {
    SeeState state{};
    state.occupied = pos.bb.occupancy();

    for (Color color : {Color::white, Color::black}) {
        for (PieceType type : SEE_ORDER) {
            state.get(type, color) = pos.bb.get(type)[color];
        }
    }

    return state;
}

static bb::U64 attackers_to(const SeeState &state, sqr_t sqr, Color by) {
    const bb::U64 pawns = state.get(PieceType::pawn, by);
    const bb::U64 knights = state.get(PieceType::knight, by);
    const bb::U64 bishops = state.get(PieceType::bishop, by);
    const bb::U64 rooks = state.get(PieceType::rook, by);
    const bb::U64 queens = state.get(PieceType::queen, by);
    const bb::U64 kings = state.get(PieceType::king, by);

    return (bb::pawn_attacks[!by][sqr] & pawns) | (bb::knight_attacks[sqr] & knights) |
           (bb::king_attacks[sqr] & kings) |
           (bb::get_bishop_attack(sqr, state.occupied) & (bishops | queens)) |
           (bb::get_rook_attack(sqr, state.occupied) & (rooks | queens));
}

static bool is_capture(const Move &move) {
    return move.flag == MoveFlag::capture || move.flag == MoveFlag::ep;
}

static bool is_quiet(const Move &move) {
    return !is_capture(move) && move.promotion_type == PieceType::none;
}

static int move_history_index(const Move &move) {
    return static_cast<int>(move.from) * 64 + static_cast<int>(move.to);
}

static int lmr_reduction(int depth, int move_index) {
    if (depth < 3 || move_index < 4) {
        return 0;
    }
    if (depth >= 6 && move_index >= 8) {
        return 2;
    }
    return 1;
}
} // namespace chess

int32_t chess::TunedPSTEngine::static_exchange_eval(
    const Position &pos, const Move &move, const EvalSettings &eval_settings
) {
    if (!is_capture(move))
        return 0;

    const Color us = pos.st->active_color;
    const Color them = !us;

    const sqr_t from = move.from;
    const sqr_t target = move.to;

    const PieceType mover = pos.board[from].type;
    const PieceType victim = move.flag == MoveFlag::ep ? PieceType::pawn : pos.board[target].type;

    const int captured_sqr_index =
        static_cast<int>(target) + (move.flag == MoveFlag::ep ? (us == Color::white ? -8 : 8) : 0);

    const sqr_t captured_sqr = static_cast<sqr_t>(captured_sqr_index);

    const bb::U64 from_bb = bb::bb(from);
    const bb::U64 target_bb = bb::bb(target);
    const bb::U64 captured_bb = bb::bb(captured_sqr);

    SeeState state = make_see_state(pos);

    std::array<int, 32> gain{};
    int depth = 0;

    gain[0] = move_piece_value(victim, eval_settings);

    PieceType occupant = mover;
    if (move.promotion_type != PieceType::none) {
        gain[0] += move_piece_value(move.promotion_type, eval_settings) -
                   move_piece_value(PieceType::pawn, eval_settings);
        occupant = move.promotion_type;
    }

    state.get(victim, them) &= ~captured_bb;
    state.get(mover, us) &= ~from_bb;
    state.get(occupant, us) |= target_bb;

    state.occupied &= ~from_bb;
    state.occupied &= ~captured_bb;
    state.occupied |= target_bb;

    Color side = them;

    while (depth + 1 < static_cast<int>(gain.size())) {
        const bb::U64 attackers = attackers_to(state, target, side);

        PieceType attacker_type = PieceType::none;
        sqr_t attacker_sqr = 0;
        for (PieceType type : SEE_ORDER) {
            const bb::U64 candidates = attackers & state.get(type, side);
            if (candidates) {
                attacker_type = type;
                attacker_sqr = lsb(candidates);
                break;
            }
        }

        if (attacker_type == PieceType::none)
            break;

        const bb::U64 attacker_bb = bb::bb(attacker_sqr);

        const bool promotes =
            attacker_type == PieceType::pawn && (side == Color::white ? target >= 56 : target < 8);

        const PieceType landed_type = promotes ? PieceType::queen : attacker_type;

        const int promotion_bonus = promotes ? move_piece_value(PieceType::queen, eval_settings) -
                                                   move_piece_value(PieceType::pawn, eval_settings)
                                             : 0;

        if (attacker_type == PieceType::king) {
            SeeState after = state;
            after.get(occupant, !side) &= ~target_bb;
            after.get(PieceType::king, side) &= ~attacker_bb;
            after.get(PieceType::king, side) |= target_bb;
            after.occupied &= ~attacker_bb;

            if (attackers_to(after, target, !side))
                break;
        }

        ++depth;
        gain[depth] = move_piece_value(occupant, eval_settings) + promotion_bonus - gain[depth - 1];

        state.get(occupant, !side) &= ~target_bb;
        state.get(attacker_type, side) &= ~attacker_bb;
        state.get(landed_type, side) |= target_bb;

        state.occupied &= ~attacker_bb;

        occupant = landed_type;

        if (attacker_type == PieceType::king)
            break;

        side = !side;
    }

    while (depth > 0) {
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
        --depth;
    }

    return gain[0];
}

chess::TunedPSTEngine::MovePicker::MovePicker(
    const Position &pos, const MoveStack<> &legal_moves, const Move &tt_move, const Move &pv_move,
    int ply, Color side, const MoveHeuristics &heuristics, const EvalSettings &eval_settings,
    const SearchSettings &search_settings
)
    : m_pos(pos), m_side(side), m_heuristics(heuristics), m_quiet_index(0),
      eval_settings(eval_settings), search_settings(search_settings) {
    if (contains(legal_moves, tt_move)) {
        m_tt_move = tt_move;
        m_has_tt_move = true;
    }

    if (contains(legal_moves, pv_move) && (!m_has_tt_move || !(pv_move == m_tt_move))) {
        m_pv_move = pv_move;
        m_has_pv_move = true;
        m_pv_move_pending = true;
    }

    for (const Move &move : legal_moves) {
        if (m_has_tt_move && move == m_tt_move)
            continue;

        if (m_has_pv_move && move == m_pv_move)
            continue;

        if (move.promotion_type != PieceType::none) {
            m_good_captures.push(move);
            continue;
        }

        if (is_capture(move)) {
            if (capture_order_score(move) >= 0)
                m_good_captures.push(move);
            else
                m_bad_captures.push(move);
            continue;
        }

        bool is_killer = false;

        if (ply >= 0 && ply < search_settings.max_killer_ply && is_quiet(move)) {
            for (const Move &killer : m_heuristics.killers[ply]) {
                if (killer == move) {
                    m_killers.push(move);
                    is_killer = true;
                    break;
                }
            }
        }

        if (!is_killer)
            m_quiets.push(move);
    }

    std::sort(m_good_captures.begin(), m_good_captures.end(), [&](const Move &a, const Move &b) {
        return tactical_score(a) > tactical_score(b);
    });

    std::sort(m_quiets.begin(), m_quiets.end(), [&](const Move &a, const Move &b) {
        return m_heuristics.history[m_side][move_history_index(a)] >
               m_heuristics.history[m_side][move_history_index(b)];
    });

    std::sort(m_bad_captures.begin(), m_bad_captures.end(), [&](const Move &a, const Move &b) {
        return tactical_score(a) > tactical_score(b);
    });
}

bool chess::TunedPSTEngine::MovePicker::next(Move &move) {
    if (m_has_tt_move) {
        m_has_tt_move = false;
        move = m_tt_move;
        return true;
    }

    if (m_pv_move_pending) {
        m_pv_move_pending = false;
        move = m_pv_move;
        return true;
    }

    if (m_good_index < m_good_captures.size()) {
        move = m_good_captures[m_good_index++];
        return true;
    }

    if (m_killer_index < m_killers.size()) {
        move = m_killers[m_killer_index++];
        return true;
    }

    if (!m_quiets.is_empty() && m_quiet_index < m_quiets.size()) {
        move = m_quiets[m_quiet_index++];
        return true;
    }

    if (m_bad_index < m_bad_captures.size()) {
        move = m_bad_captures[m_bad_index++];
        return true;
    }

    return false;
}

bool chess::TunedPSTEngine::MovePicker::contains(const MoveStack<> &moves, const Move &target) {
    if (target == Move::none())
        return false;

    return std::find(moves.begin(), moves.end(), target) != moves.end();
}

int chess::TunedPSTEngine::MovePicker::capture_order_score(const Move &move) const {
    return static_exchange_eval(m_pos, move, eval_settings);
}

int chess::TunedPSTEngine::MovePicker::tactical_score(const Move &move) const {
    if (move.promotion_type != PieceType::none) {
        return 100000 + move_piece_value(move.promotion_type, eval_settings);
    }

    const PieceType attacker = m_pos.board[move.from].type;
    const PieceType victim =
        move.flag == MoveFlag::ep ? PieceType::pawn : m_pos.board[move.to].type;

    return 16 * move_piece_value(victim, eval_settings) - move_piece_value(attacker, eval_settings);
}

void chess::TunedPSTEngine::update_pv(SearchContext &ctx, uint16_t ply, const Move &move) {
    if (ply >= SearchSettings::max_ply - 1)
        return;
    ctx.pv[ply][0] = move;

    int child_length = ctx.pv_length[ply + 1];

    for (int i = 0; i < child_length; ++i) {
        ctx.pv[ply][i + 1] = ctx.pv[ply + 1][i];
    }

    ctx.pv_length[ply] = child_length + 1;
}

namespace chess {
static bb::U64 get_advanced_passed(const chess::Position &pos, Color color) {
    const bb::U64 own_pawns = pos.bb.get(PieceType::pawn)[color];
    const bb::U64 enemy_pawns = pos.bb.get(PieceType::pawn)[!color];

    bb::U64 pawns = own_pawns & (color == Color::white ? bb::ranks[5] | bb::ranks[6]
                                                       : bb::ranks[1] | bb::ranks[2]);
    bb::U64 passed = 0ULL;
    while (pawns) {
        const sqr_t sqr = lsb(pawns);
        pawns &= pawns - 1;

        if (!(bb::passed_pawn_masks[color][sqr] & enemy_pawns))
            passed |= bb::bb(sqr);
    }
    return passed;
}
} // namespace chess

int32_t chess::TunedPSTEngine::quiescence(
    int32_t alpha, int32_t beta, SearchContext &ctx, int ply, int qply,
    const EvalSettings &eval_settings, const SearchSettings &search_settings
) {
    if (should_stop(ctx, search_settings))
        return 0;

    const bool in_check = pos.st->checkers != 0;

    auto moves = gen_moves<GenType::legal>(pos);
    switch (pos.get_game_end(moves)) {
    case GameEnd::checkmate: return -search_settings.mate_score + ply;
    case GameEnd::proceed:   break;
    default:                 return 0;
    }

    if (ply >= search_settings.max_ply) {
        if (in_check)
            return 0;
        return evaluate(eval_settings);
    }

    if (qply >= search_settings.max_qply && !in_check)
        return evaluate(eval_settings);

    if (!in_check) {
        const int32_t stand_pat = evaluate(eval_settings);
        if (stand_pat >= beta)
            return stand_pat;
        alpha = std::max(alpha, stand_pat);
    }

    const bb::U64 advanced_passed = get_advanced_passed(pos, pos.st->active_color);
    for (const auto &move : moves) {
        const bool is_capture = move.flag == MoveFlag::capture || move.flag == MoveFlag::ep;

        if (!in_check && !is_capture && move.promotion_type == PieceType::none &&
            (bb::bb(move.from) & advanced_passed) == 0)
            continue;

        State st;
        pos.do_move(move, st);
        const int32_t score =
            -quiescence(-beta, -alpha, ctx, ply + 1, qply + 1, eval_settings, search_settings);
        pos.undo_move(move);

        if (ctx.stopped)
            return 0;

        if (score >= beta)
            return score;

        alpha = std::max(alpha, score);
    }

    return alpha;
}

int32_t chess::TunedPSTEngine::score_to_tt(
    int32_t score, uint16_t ply, const SearchSettings &search_settings
) {
    if (score >= search_settings.mate_threshold)
        return score + ply;

    if (score <= -search_settings.mate_threshold)
        return score - ply;

    return score;
}

int32_t chess::TunedPSTEngine::score_from_tt(
    int32_t score, uint16_t ply, const SearchSettings &search_settings
) {
    if (score >= search_settings.mate_threshold)
        return score - ply;

    if (score <= -search_settings.mate_threshold)
        return score + ply;

    return score;
}

int32_t chess::TunedPSTEngine::negamax(
    uint16_t depth, uint16_t ply, int32_t alpha, int32_t beta, SearchContext &ctx,
    bool prev_move_was_null, const EvalSettings &eval_settings,
    const SearchSettings &search_settings
) {
    if (should_stop(ctx, search_settings))
        return 0;

    if (ply >= search_settings.max_ply) {
        const auto moves = gen_moves<GenType::legal>(pos);
        switch (pos.get_game_end(moves)) {
        case GameEnd::checkmate: return -search_settings.mate_score + ply;
        case GameEnd::proceed:   break;
        default:                 return 0;
        }
        if (pos.st->checkers != 0)
            return 0;
        return evaluate(eval_settings);
    }

    ctx.pv_length[ply] = 0;

    if (depth == 0)
        return evaluate(eval_settings);
    // return quiescence(alpha, beta, ctx, ply, 0, eval_settings, search_settings);

    auto moves = chess::gen_moves<GenType::legal>(pos);
    switch (pos.get_game_end(moves)) {
    case GameEnd::proceed:   break;
    case GameEnd::checkmate: return -search_settings.mate_score + ply;
    default:                 return 0;
    }

    if (!prev_move_was_null && depth >= 3 && pos.st->checkers == 0 &&
        pos.bb.get(PieceType::pawn)[pos.st->active_color] !=
            0 /* TODO: zugzwang can still occur */) {
        State st;
        pos.do_null_move(st);
        int32_t score = -negamax(
            depth - 3, ply + 1, -beta, -beta + 1, ctx, true, eval_settings, search_settings
        );
        pos.undo_null_move();

        if (ctx.stopped)
            return 0;

        if (score >= beta)
            return score;
    }

    const int32_t og_alpha = alpha;
    const int32_t og_beta = beta;

    auto *entry = tt.probe(pos.st->hash);
    Move tt_move = Move::none();

    if (entry != nullptr && entry->key == pos.st->hash) {
        tt_move = entry->best_move;
        if (entry->depth >= depth) {
            const int32_t tt_score = score_from_tt(entry->score, ply, search_settings);
            switch (entry->bound) {
            case TTBound::exact: return tt_score;
            case TTBound::lower:
                if (tt_score >= beta)
                    return tt_score;

                alpha = std::max(alpha, tt_score);
                break;
            case TTBound::upper:
                if (tt_score <= alpha)
                    return tt_score;

                beta = std::min(beta, tt_score);
                break;
            }
        }
    }

    Move pv_move = Move::none();

    if (ply < ctx.prev_pv_length)
        pv_move = ctx.prev_pv[ply];

    const bool in_check = pos.st->checkers != 0;

    MovePicker picker(
        pos,
        moves,
        tt_move,
        pv_move,
        ply,
        pos.st->active_color,
        ctx.heuristics,
        eval_settings,
        search_settings
    );

    int32_t best = -search_settings.inf;
    Move best_move = Move::none();
    int move_index = 0;

    Move move;
    while (picker.next(move)) {
        move_index++;

        State st;
        pos.do_move(move, st);

        int32_t score;
        if (move_index == 1) {
            score = -negamax(
                depth - 1, ply + 1, -beta, -alpha, ctx, false, eval_settings, search_settings
            );
        } else {
            const int reduction =
                !in_check && is_quiet(move) ? lmr_reduction(depth, move_index) : 0;
            score = -negamax(
                depth - 1 - reduction,
                ply + 1,
                -alpha - 1,
                -alpha,
                ctx,
                false,
                eval_settings,
                search_settings
            );
            if (reduction > 0 && score > alpha) {
                score = -negamax(
                    depth - 1,
                    ply + 1,
                    -alpha - 1,
                    -alpha,
                    ctx,
                    false,
                    eval_settings,
                    search_settings
                );
            }
            if (score > alpha && score < beta) {
                score = -negamax(
                    depth - 1, ply + 1, -beta, -alpha, ctx, false, eval_settings, search_settings
                );
            }
        }

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

        if (alpha >= beta) {
            if (is_quiet(move) && ply < search_settings.max_killer_ply) {
                auto &killers = ctx.heuristics.killers[ply];
                if (!(killers[0] == move)) {
                    killers[1] = killers[0];
                    killers[0] = move;
                }

                const int index = move_history_index(move);
                const int bonus = depth * depth;

                auto &history = ctx.heuristics.history[pos.st->active_color][index];
                history += bonus - history * bonus / 16384;
                history = std::clamp(history, -16384, 16384);
            }

            break;
        }
    }

    TTBound bound;
    if (best <= og_alpha) {
        bound = TTBound::upper;
    } else if (best >= beta) {
        bound = TTBound::lower;
    } else {
        bound = TTBound::exact;
    }
    tt.set(
        pos.st->hash,
        TTentry{
            .key = pos.st->hash,
            .depth = static_cast<int16_t>(depth),
            .score = score_to_tt(best, ply, search_settings),
            .bound = bound,
            .best_move = best_move
        }
    );

    return best;
}

int32_t chess::TunedPSTEngine::root_search(
    MoveStack<> &root_moves, uint16_t depth, SearchContext &ctx, int32_t alpha, int32_t beta,
    Move &best_move, const EvalSettings &eval_settings, const SearchSettings &search_settings
) {
    int32_t best_score = -search_settings.inf;

    Move iteration_best = Move::none();
    Move pv_move = Move::none();
    if (ctx.prev_pv_length > 0)
        pv_move = ctx.prev_pv[0];

    Move tt_move = get_tt_move(ctx);

    MovePicker picker(
        pos,
        root_moves,
        tt_move,
        pv_move,
        0,
        pos.st->active_color,
        ctx.heuristics,
        eval_settings,
        search_settings
    );

    Move move;
    while (picker.next(move)) {
        if (Clock::now() >= ctx.deadline) {
            ctx.stopped = true;
            return 0;
        }

        State st;
        pos.do_move(move, st);
        const int32_t score =
            -negamax(depth - 1, 1, -beta, -alpha, ctx, false, eval_settings, search_settings);
        pos.undo_move(move);

        if (ctx.stopped)
            return 0;

        if (score > best_score) {
            best_score = score;
            iteration_best = move;
        }

        if (score > alpha) {
            alpha = score;

            ctx.pv[0][0] = move;
            for (int i = 0; i < ctx.pv_length[1]; ++i)
                ctx.pv[0][i + 1] = ctx.pv[1][i];
            ctx.pv_length[0] = ctx.pv_length[1] + 1;
        }

        if (alpha >= beta)
            break;
    }

    best_move = iteration_best;
    return best_score;
}

chess::TunedPSTEngine::SearchResult chess::TunedPSTEngine::search(
    std::chrono::milliseconds time, const EvalSettings &eval_settings,
    const SearchSettings &search_settings
) {
    SearchContext ctx{.deadline = Clock::now() + time, .stopped = false, .nodes = 0};

    SearchResult result{.best_move = Move::none(), .score = 0, .depth = 0};

    auto root_moves = chess::gen_moves<GenType::legal>(pos);
    if (pos.get_game_end(root_moves) != GameEnd::proceed) {
        return result;
    }
    result.best_move = root_moves[0];
    result.score = evaluate(eval_settings);
    for (uint16_t depth = 1; depth < std::numeric_limits<uint16_t>().max(); depth++) {
        if (Clock::now() >= ctx.deadline)
            break;

        ctx.stopped = false;

        ctx.prev_pv_length = ctx.pv_length[0];
        for (int i = 0; i < ctx.prev_pv_length; ++i)
            ctx.prev_pv[i] = ctx.pv[0][i];
        ctx.pv_length.fill(0);

        int32_t delta = search_settings.aspiration_window;

        int32_t alpha =
            depth > 1 ? std::max(-search_settings.inf, result.score - delta) : -search_settings.inf;
        int32_t beta =
            depth > 1 ? std::min(search_settings.inf, result.score + delta) : search_settings.inf;

        int32_t iteration_score = -search_settings.inf;
        Move iteration_best = result.best_move;
        while (true) {
            ctx.pv_length.fill(0);
            iteration_best = result.best_move;

            iteration_score = root_search(
                root_moves, depth, ctx, alpha, beta, iteration_best, eval_settings, search_settings
            );

            if (ctx.stopped)
                break;

            if (iteration_score <= alpha) {
                beta = alpha + (beta - alpha) / 2;
                alpha = std::max(-search_settings.inf, iteration_score - delta);
            } else if (iteration_score >= beta) {
                beta = std::min(search_settings.inf, iteration_score + delta);
            } else {
                break;
            }

            delta = delta >= search_settings.inf / 2 ? search_settings.inf : delta * 2;
        }

        if (ctx.stopped)
            break;

        result.best_move = iteration_best;
        result.score = iteration_score;
        result.depth = depth;
    }

    return result;
}

chess::TunedPSTEngine::SearchResult chess::TunedPSTEngine::search(std::chrono::milliseconds time) {
    static constexpr EvalSettings eval_defaults{};
    static constexpr SearchSettings search_defaults{};
    return search(time, eval_defaults, search_defaults);
}
