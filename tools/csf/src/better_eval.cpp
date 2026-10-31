#include "better_eval.hpp"
#include "constbb.hpp"
#include "genmoves.hpp"
#include "misc.hpp"
#include "piece_color.hpp"
#include "position.hpp"
#include "tranpostable.hpp"

#include <algorithm>
#include <limits>
#include <utility>

chess::BetterEvalEngine::BetterEvalEngine() : st_stack{}, pos{}, moves{}, tt{} {}
chess::BetterEvalEngine::BetterEvalEngine(std::string_view fen, Position::FenParseErr &out_err)
    : st_stack{}, pos{}, moves{}, tt{} {
    out_err = parse_fen(fen);
}

chess::Position::FenParseErr chess::BetterEvalEngine::parse_fen(std::string_view fen) {
    const bool push_new = st_stack.size() == 0;
    if (push_new) { st_stack.emplace(); }
    chess::State old_st = st_stack[0];
    auto err = pos.parse_fen(fen, st_stack[0]);
    if (err != Position::FenParseErr::ok) {
        st_stack[0] = old_st;
        if (push_new) { st_stack.clear(); }
    } else {
        st_stack.resize(1);
    }
    return err;
}

chess::Position &chess::BetterEvalEngine::position() { return pos; }
const chess::Position &chess::BetterEvalEngine::position() const { return pos; }

const chess::MoveStack<> &chess::BetterEvalEngine::gen_legal_moves() {
    return moves = gen_moves<GenType::legal>(pos);
}

void chess::BetterEvalEngine::do_move(const Move &move) { pos.do_move(move, st_stack.emplace()); }

void chess::BetterEvalEngine::undo_move(const Move &move) {
    pos.undo_move(move);
    st_stack.pop();
}

namespace chess {
static uint8_t doubled(const Position &pos, Color color) {
    bb::U64 pawns = pos.bb.get(PieceType::pawn)[color];
    int8_t doubled = 0;
    for (uint8_t file = 0; file < 8; ++file) {
        const int count = popcount(pawns & bb::files[file]);
        if (count > 1) doubled += count - 1;
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

        if (!(pawns & file)) { continue; }

        bb::U64 adjacent = 0;
        if (f > 0) adjacent |= bb::files[f - 1];
        if (f < 7) adjacent |= bb::files[f + 1];

        if (!(pawns & adjacent)) { count += popcount(pawns & file); }
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
        const sqr_t sqr = poplsb(pawns);

        if (!(bb::passed_pawn_masks[color][sqr] & enemy_pawns)) passed |= bb::bb(sqr);

        if (color == Color::white ? sqr >= 56 : sqr < 8) continue;

        const sqr_t stop = color == Color::white ? sqr + 8 : sqr - 8;

        const bb::U64 stop_bb = bb::bb(stop);

        if (!(empty & stop_bb)) continue;

        if (!(enemy_attacks & stop_bb)) continue;

        if (own_attacks & stop_bb) continue;

        backward |= bb::bb(sqr);
    }

    return BackwardPassed{
        .backward_count = static_cast<int16_t>(popcount(backward)),
        .passed_count = static_cast<int16_t>(popcount(passed))
    };
}

constexpr int MAX_PHASE = 24;

enum class GamePhase { middlegame, endgame };

static int8_t count_pieces(const Position &pos, PieceType type) {
    return popcount(pos.bb.get(type)[Color::white]) + popcount(pos.bb.get(type)[Color::black]);
}

static int32_t calculate_game_phase(const Position &pos) {
    int32_t phase = 0;

    phase += 4 * count_pieces(pos, PieceType::queen);
    phase += 2 * count_pieces(pos, PieceType::rook);
    phase += count_pieces(pos, PieceType::bishop);
    phase += count_pieces(pos, PieceType::knight);

    return std::min(phase, MAX_PHASE);
}

static int32_t evaluate_piece_sqr(
    const Position &pos, Color perspective,
    const PieceArray<BetterEvalEngine::PieceSquareTables> &pst
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
                const sqr_t sqr = poplsb(pieces);
                const sqr_t rel_sqr_ = rel_sqr(sqr, color);

                mg_score += sign * pst[type].middlegame[rel_sqr_];
                eg_score += sign * pst[type].endgame[rel_sqr_];
            }
        }
    }

    const int phase = calculate_game_phase(pos);

    return (mg_score * phase + eg_score * (MAX_PHASE - phase)) / MAX_PHASE;
}

static bool has_bishop_pair(const Position &pos, Color color) {
    const bb::U64 bishops = pos.bb.get(PieceType::bishop)[color];
    return bishops & bb::light_sqrs() && bishops & bb::dark_sqrs();
}

static int32_t
king_shield(const Position &pos, Color color, const BetterEvalEngine::EvalSettings &eval) {
    int32_t score = 0;
    Piece pawn(PieceType::pawn, Color::white);

    file_t king_file = lsb(pos.bb.get(PieceType::king)[Color::white]) % 8;
    rank_t start_pawn_rank = color == Color::white ? 1 : 6;
    rank_t push_pawn_rank = color == Color::white ? 2 : 5;
    std::array<file_t, 3> files;
    if (king_file > File::E) {
        files = {File::F, File::G, File::H};
    } else if (king_file < File::D) {
        files = {File::A, File::B, File::C};
    } else {
        return score;
    }
    for (file_t file : files) {
        if (pos.board[file + (8 * start_pawn_rank)] == pawn) score += eval.king_shield_2_bonus;
        if (pos.board[file + (8 * push_pawn_rank)] == pawn) score += eval.king_shield_3_bonus;
    }

    return score;
}

static inline int32_t returning_bishop(const Position &pos, Color color) {
    if ((pos.board[rel_sqr(g1, color)] == Piece{PieceType::king, color} &&
         pos.board[rel_sqr(f1, color)] == Piece{PieceType::bishop, color}) ||
        (pos.board[rel_sqr(b1, color)] == Piece{PieceType::king, color} &&
         pos.board[rel_sqr(c1, color)] == Piece{PieceType::bishop, color})) {
        return 1;
    }

    return 0;
}

static inline int32_t eval_bishop(
    const Position &pos, sqr_t sqr, int32_t phase, int32_t &king_attackers_count,
    int32_t &king_attackers_weights, const BetterEvalEngine::EvalSettings &eval
) {
    int32_t score = 0;
    Color color = pos.board[sqr].color;

    const bb::U64 occ = pos.bb.occupancy();
    bb::U64 attacks = bb::get_bishop_attack(sqr, occ);
    attacks &= ~pos.bb.every_type()[color] | ~pos.st->pawn_ctrl;
    uint8_t mobility = popcount(attacks);
    int8_t mg_mobility =
        eval.mg_bishop_mobility_multiplier * (mobility - eval.mobility_0_bishop_penalty);
    int8_t eg_mobility =
        eval.eg_bishop_mobility_multiplier * (mobility - eval.mobility_0_bishop_penalty);

    score += (mg_mobility * phase + eg_mobility * (MAX_PHASE - phase)) / MAX_PHASE;

    assert(poplsb(pos.bb.get(PieceType::king)[!color]) == 1);
    bb::U64 king_atks_bb = attacks & bb::near_sqrs[lsb(pos.bb.get(PieceType::king)[!color])];
    uint8_t king_atks_count = popcount(king_atks_bb);
    king_attackers_count += king_atks_count;
    king_attackers_weights += eval.bishop_king_attackers_weight * king_atks_count;

    return score;
}

static inline int32_t eval_rook(
    const Position &pos, sqr_t sqr, int32_t phase, int32_t &king_attackers_count,
    int32_t &king_attackers_weights, const BetterEvalEngine::EvalSettings &eval
) {
    int32_t score = 0;
    Color color = pos.board[sqr].color;

    uint8_t mobility = 0;
    int8_t mg_mobility = 0;
    int8_t eg_mobility = 0;

    const bb::U64 enemy_pawns = pos.bb.get(PieceType::pawn)[!color];
    if (bb::bb(sqr) & bb::rel_ranks[color][Rank::R7] &&
        (enemy_pawns & bb::rel_ranks[color][Rank::R7] ||
         pos.bb.get(PieceType::king)[!color] & bb::rel_ranks[color][Rank::R8])) {
        mg_mobility += eval.mg_rook_7th_bonus;
        eg_mobility += eval.eg_rook_7th_bonus;
    }

    file_t file = sqr % 8;
    bb::U64 file_bb = bb::files[file];
    if (!(pos.bb.get(PieceType::pawn)[color] & file_bb)) {
        if (!(enemy_pawns & file_bb)) {
            mg_mobility += eval.rook_full_open_file_bonus;
            eg_mobility += eval.rook_full_open_file_bonus;
        } else {
            mg_mobility += eval.rook_half_open_file_bonus;
            eg_mobility += eval.rook_half_open_file_bonus;
        }
    }

    const bb::U64 occ = pos.bb.occupancy();
    bb::U64 attacks = bb::get_rook_attack(sqr, occ);
    attacks &= ~pos.bb.every_type()[color] | ~pos.st->pawn_ctrl;
    mobility += popcount(attacks);
    mg_mobility += eval.mg_rook_mobility_multiplier * (mobility - eval.mobility_0_rook_penalty);
    eg_mobility += eval.eg_rook_mobility_multiplier * (mobility - eval.mobility_0_rook_penalty);

    score += (mg_mobility * phase + eg_mobility * (MAX_PHASE - phase)) / MAX_PHASE;

    assert(poplsb(pos.bb.get(PieceType::king)[!color]) == 1);
    bb::U64 king_atks_bb = attacks & bb::near_sqrs[lsb(pos.bb.get(PieceType::king)[!color])];
    uint8_t king_atks_count = popcount(king_atks_bb);
    king_attackers_count += king_atks_count;
    king_attackers_weights += eval.rook_king_attackers_weight * king_atks_count;

    return score;
}

static inline int32_t eval_queen(
    const Position &pos, sqr_t sqr, int32_t phase, int32_t &king_attackers_count,
    int32_t &king_attackers_weights, const BetterEvalEngine::EvalSettings &eval
) {
    int32_t score = 0;
    Color color = pos.board[sqr].color;

    struct Undeveloped {
        sqr_t rel_sqr;
        PieceType type;
        Undeveloped(sqr_t rel_sqr, PieceType type) : rel_sqr(rel_sqr), type(type) {}
    };
    rank_t rank = sqr / 8;
    if ((color == Color::white && rank > 1) || (color == Color::black && rank < 6)) {
        for (const auto &undeveloped : {
                 Undeveloped{b1, PieceType::knight},
                 Undeveloped{c1, PieceType::bishop},
                 Undeveloped{f1, PieceType::bishop},
                 Undeveloped{g1, PieceType::knight},
        }) {
            if (pos.board[rel_sqr(undeveloped.rel_sqr, color)] == Piece{undeveloped.type, color})
                score -= eval.early_developed_queen_penalty;
        }
    }

    const bb::U64 occ = pos.bb.occupancy();
    bb::U64 attacks = bb::get_rook_attack(sqr, occ) | bb::get_bishop_attack(sqr, occ);
    attacks &= ~pos.bb.every_type()[color] | ~pos.st->pawn_ctrl;
    uint8_t mobility = popcount(attacks);
    int8_t mg_mobility =
        eval.mg_queen_mobility_multiplier * (mobility - eval.mobility_0_queen_penalty);
    int8_t eg_mobility =
        eval.eg_queen_mobility_multiplier * (mobility - eval.mobility_0_queen_penalty);

    score += (mg_mobility * phase + eg_mobility * (MAX_PHASE - phase)) / MAX_PHASE;

    assert(poplsb(pos.bb.get(PieceType::king)[!color]) == 1);
    bb::U64 king_atks_bb = attacks & bb::near_sqrs[lsb(pos.bb.get(PieceType::king)[!color])];
    uint8_t king_atks_count = popcount(king_atks_bb);
    king_attackers_count += king_atks_count;
    king_attackers_weights += eval.queen_king_attackers_weight * king_atks_count;

    return score;
}

static inline int32_t
eval_pieces(const Position &pos, Color color, const BetterEvalEngine::EvalSettings &eval) {
    int32_t score = 0;

    int32_t phase = calculate_game_phase(pos);

    int32_t king_attackers_count = 0;
    int32_t king_attackers_weights = 0;

    bb::U64 occ = pos.bb.every_type()[color] & ~pos.bb.get(PieceType::pawn)[color];
    while (occ != 0) {
        sqr_t sqr = poplsb(occ);
        switch (pos.board[sqr].type) {
        case PieceType::bishop:
            score +=
                eval_bishop(pos, sqr, phase, king_attackers_count, king_attackers_weights, eval);
            break;
        case PieceType::rook:
            score += eval_rook(pos, sqr, phase, king_attackers_count, king_attackers_weights, eval);
            break;
        case PieceType::queen:
            score +=
                eval_queen(pos, sqr, phase, king_attackers_count, king_attackers_weights, eval);
            break;
        default: break;
        }
    }

    int32_t king_danger = king_attackers_count * king_attackers_weights;
    int32_t mg_king_danger = king_danger * king_danger / 4096;
    int32_t eg_king_danger = king_danger / 16;

    score += (mg_king_danger * phase + eg_king_danger * (MAX_PHASE - phase)) / MAX_PHASE;

    return score;
}
} // namespace chess

int32_t chess::BetterEvalEngine::evaluate(const EvalSettings &eval) const {
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

    int32_t score = eval.king_val * popcount_diff(PieceType::king) +
                    eval.queen_val * popcount_diff(PieceType::queen) +
                    adj(eval.rook_val, eval.rook_adj, PieceType::rook) +
                    eval.bishop_val * popcount_diff(PieceType::bishop) +
                    adj(eval.knight_val, eval.knight_adj, PieceType::knight) +
                    eval.pawn_val * popcount_diff(PieceType::pawn);

    const auto get_diff = [&]<typename Fn, typename... Args>(Fn func, Args &&...args) {
        return static_cast<int32_t>(func(pos, color, std::forward<Args>(args)...)) -
               static_cast<int32_t>(func(pos, !color, std::forward<Args>(args)...));
    };

    auto our_backward_passed = get_backward_passed(pos, color);
    auto their_backward_passed = get_backward_passed(pos, enemy);
    score -= eval.blocked_penalty * get_diff(blocked);
    score -= eval.doubled_penalty * get_diff(doubled);
    score -= eval.isolated_penalty * get_diff(isolated);
    score -= eval.backward_penalty *
             (our_backward_passed.backward_count - their_backward_passed.backward_count);

    score +=
        eval.passed_bonus * (our_backward_passed.passed_count - their_backward_passed.passed_count);
    const int our_pair = has_bishop_pair(pos, color) ? 1 : 0;
    const int their_pair = has_bishop_pair(pos, enemy) ? 1 : 0;
    score += eval.bishop_pair_bonus * (our_pair - their_pair);
    score += get_diff(king_shield, eval);
    score += eval.returning_bishop_bonus * get_diff(returning_bishop);
    score += get_diff(eval_pieces, eval);

    score += evaluate_piece_sqr(pos, color, eval.pst);

    return score;
}

int32_t chess::BetterEvalEngine::evaluate() const {
    static constexpr EvalSettings defaults{};
    return evaluate(defaults);
}

bool chess::BetterEvalEngine::should_stop(SearchContext &ctx, const SearchSettings &search) {
    ctx.nodes++;

    assert(search.time_check_interval > 0);
    if ((ctx.nodes % search.time_check_interval) != 0) return false;

    if (Clock::now() >= *ctx.deadline) {
        ctx.stopped = true;
        return true;
    }

    return false;
}

chess::Move chess::BetterEvalEngine::get_tt_move(SearchContext &ctx) {
    if (auto *entry = tt.probe(pos.st->hash)) {
        if (entry->key == pos.st->hash) return entry->best_move;
    }

    return Move::none();
}

int32_t chess::BetterEvalEngine::move_piece_value(PieceType type, const EvalSettings &eval) {
    switch (type) {
    case PieceType::pawn:   return eval.pawn_val;
    case PieceType::knight: return eval.knight_val;
    case PieceType::bishop: return eval.bishop_val;
    case PieceType::rook:   return eval.rook_val;
    case PieceType::queen:  return eval.queen_val;
    case PieceType::king:   return eval.king_val;
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
        for (PieceType type : SEE_ORDER) { state.get(type, color) = pos.bb.get(type)[color]; }
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
    if (depth < 3 || move_index < 4) { return 0; }
    if (depth >= 6 && move_index >= 8) { return 2; }
    return 1;
}
} // namespace chess

int32_t chess::BetterEvalEngine::static_exchange_eval(
    const Position &pos, const Move &move, const EvalSettings &eval
) {
    if (!is_capture(move)) return 0;

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

    gain[0] = move_piece_value(victim, eval);

    PieceType occupant = mover;
    if (move.promotion_type != PieceType::none) {
        gain[0] +=
            move_piece_value(move.promotion_type, eval) - move_piece_value(PieceType::pawn, eval);
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

        if (attacker_type == PieceType::none) break;

        const bb::U64 attacker_bb = bb::bb(attacker_sqr);

        const bool promotes =
            attacker_type == PieceType::pawn && (side == Color::white ? target >= 56 : target < 8);

        const PieceType landed_type = promotes ? PieceType::queen : attacker_type;

        const int promotion_bonus = promotes ? move_piece_value(PieceType::queen, eval) -
                                                   move_piece_value(PieceType::pawn, eval)
                                             : 0;

        if (attacker_type == PieceType::king) {
            SeeState after = state;
            after.get(occupant, !side) &= ~target_bb;
            after.get(PieceType::king, side) &= ~attacker_bb;
            after.get(PieceType::king, side) |= target_bb;
            after.occupied &= ~attacker_bb;

            if (attackers_to(after, target, !side)) break;
        }

        ++depth;
        gain[depth] = move_piece_value(occupant, eval) + promotion_bonus - gain[depth - 1];

        state.get(occupant, !side) &= ~target_bb;
        state.get(attacker_type, side) &= ~attacker_bb;
        state.get(landed_type, side) |= target_bb;

        state.occupied &= ~attacker_bb;

        occupant = landed_type;

        if (attacker_type == PieceType::king) break;

        side = !side;
    }

    while (depth > 0) {
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
        --depth;
    }

    return gain[0];
}

chess::BetterEvalEngine::MovePicker::MovePicker(
    const Position &pos, const MoveStack<> &legal_moves, const Move &tt_move, const Move &pv_move,
    int ply, Color side, const MoveHeuristics &heuristics, const EvalSettings &eval,
    const SearchSettings &search
)
    : m_pos(pos), m_side(side), m_heuristics(heuristics), m_quiet_index(0), eval(eval),
      search(search) {
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
        if (m_has_tt_move && move == m_tt_move) continue;

        if (m_has_pv_move && move == m_pv_move) continue;

        if (move.promotion_type != PieceType::none) {
            m_good_captures.push(move);
            continue;
        }

        if (is_capture(move)) {
            if (capture_order_score(move) >= 0) m_good_captures.push(move);
            else
                m_bad_captures.push(move);
            continue;
        }

        bool is_killer = false;

        if (ply >= 0 && ply < search.max_killer_ply && is_quiet(move)) {
            for (const Move &killer : m_heuristics.killers[ply]) {
                if (killer == move) {
                    m_killers.push(move);
                    is_killer = true;
                    break;
                }
            }
        }

        if (!is_killer) m_quiets.push(move);
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

bool chess::BetterEvalEngine::MovePicker::next(Move &move) {
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

bool chess::BetterEvalEngine::MovePicker::contains(const MoveStack<> &moves, const Move &target) {
    if (target == Move::none()) return false;

    return std::find(moves.begin(), moves.end(), target) != moves.end();
}

int chess::BetterEvalEngine::MovePicker::capture_order_score(const Move &move) const {
    return static_exchange_eval(m_pos, move, eval);
}

int chess::BetterEvalEngine::MovePicker::tactical_score(const Move &move) const {
    if (move.promotion_type != PieceType::none) {
        return 100000 + move_piece_value(move.promotion_type, eval);
    }

    const PieceType attacker = m_pos.board[move.from].type;
    const PieceType victim =
        move.flag == MoveFlag::ep ? PieceType::pawn : m_pos.board[move.to].type;

    return 16 * move_piece_value(victim, eval) - move_piece_value(attacker, eval);
}

void chess::BetterEvalEngine::update_pv(SearchContext &ctx, uint16_t ply, const Move &move) {
    if (ply >= SearchSettings::max_ply - 1) return;
    ctx.pv[ply][0] = move;

    int child_length = ctx.pv_length[ply + 1];

    for (int i = 0; i < child_length; ++i) { ctx.pv[ply][i + 1] = ctx.pv[ply + 1][i]; }

    ctx.pv_length[ply] = child_length + 1;
}

namespace chess {
static bb::U64 get_advanced_passed(const chess::Position &pos, Color color) {
    const bb::U64 own_pawns = pos.bb.get(PieceType::pawn)[color];
    const bb::U64 enemy_pawns = pos.bb.get(PieceType::pawn)[!color];

    bb::U64 pawns = own_pawns & (color == Color::white ? bb::ranks[Rank::R6] | bb::ranks[Rank::R7]
                                                       : bb::ranks[Rank::R2] | bb::ranks[Rank::R3]);
    bb::U64 passed = 0ULL;
    while (pawns) {
        const sqr_t sqr = poplsb(pawns);

        if (!(bb::passed_pawn_masks[color][sqr] & enemy_pawns)) passed |= bb::bb(sqr);
    }
    return passed;
}
} // namespace chess

int32_t chess::BetterEvalEngine::quiescence(
    int32_t alpha, int32_t beta, SearchContext &ctx, int ply, int qply, const EvalSettings &eval,
    const SearchSettings &search
) {
    if (should_stop(ctx, search)) return 0;

    const bool in_check = pos.st->checkers != 0;

    auto moves = gen_moves<GenType::legal>(pos);
    switch (pos.get_game_end(moves)) {
    case GameEnd::checkmate: return -search.mate_score + ply;
    case GameEnd::proceed:   break;
    default:                 return 0;
    }

    if (ply >= search.max_ply) {
        if (in_check) return 0;
        return evaluate(eval);
    }

    if (qply >= search.max_qply && !in_check) return evaluate(eval);

    if (!in_check) {
        const int32_t stand_pat = evaluate(eval);
        if (stand_pat >= beta) return stand_pat;
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
        const int32_t score = -quiescence(-beta, -alpha, ctx, ply + 1, qply + 1, eval, search);
        pos.undo_move(move);

        if (ctx.stopped) return 0;

        if (score >= beta) return score;

        alpha = std::max(alpha, score);
    }

    return alpha;
}

int32_t
chess::BetterEvalEngine::score_to_tt(int32_t score, uint16_t ply, const SearchSettings &search) {
    if (score >= search.mate_threshold) return score + ply;

    if (score <= -search.mate_threshold) return score - ply;

    return score;
}

int32_t
chess::BetterEvalEngine::score_from_tt(int32_t score, uint16_t ply, const SearchSettings &search) {
    if (score >= search.mate_threshold) return score - ply;

    if (score <= -search.mate_threshold) return score + ply;

    return score;
}

int32_t chess::BetterEvalEngine::negamax(
    uint16_t depth, uint16_t ply, int32_t alpha, int32_t beta, SearchContext &ctx,
    bool prev_move_was_null, const EvalSettings &eval, const SearchSettings &search
) {
    if (should_stop(ctx, search)) return 0;

    if (ply >= search.max_ply) {
        const auto moves = gen_moves<GenType::legal>(pos);
        switch (pos.get_game_end(moves)) {
        case GameEnd::checkmate: return -search.mate_score + ply;
        case GameEnd::proceed:   break;
        default:                 return 0;
        }
        if (pos.st->checkers != 0) return 0;
        return evaluate(eval);
    }

    ctx.pv_length[ply] = 0;

    if (depth == 0) return quiescence(alpha, beta, ctx, ply, 0, eval, search);

    auto moves = chess::gen_moves<GenType::legal>(pos);
    switch (pos.get_game_end(moves)) {
    case GameEnd::proceed:   break;
    case GameEnd::checkmate: return -search.mate_score + ply;
    default:                 return 0;
    }

    if (!prev_move_was_null && depth >= 3 && pos.st->checkers == 0 &&
        pos.bb.get(PieceType::pawn)[pos.st->active_color] !=
            0 /* TODO: zugzwang can still occur */) {
        State st;
        pos.do_null_move(st);
        int32_t score = -negamax(depth - 3, ply + 1, -beta, -beta + 1, ctx, true, eval, search);
        pos.undo_null_move();

        if (ctx.stopped) return 0;

        if (score >= beta) return score;
    }

    const int32_t og_alpha = alpha;
    const int32_t og_beta = beta;

    auto *entry = tt.probe(pos.st->hash);
    Move tt_move = Move::none();

    if (entry != nullptr && entry->key == pos.st->hash) {
        tt_move = entry->best_move;
        if (entry->depth >= depth) {
            const int32_t tt_score = score_from_tt(entry->score, ply, search);
            switch (entry->bound) {
            case TTBound::exact: return tt_score;
            case TTBound::lower:
                if (tt_score >= beta) return tt_score;

                alpha = std::max(alpha, tt_score);
                break;
            case TTBound::upper:
                if (tt_score <= alpha) return tt_score;

                beta = std::min(beta, tt_score);
                break;
            }
        }
    }

    Move pv_move = Move::none();

    if (ply < ctx.prev_pv_length) pv_move = ctx.prev_pv[ply];

    const bool in_check = pos.st->checkers != 0;

    MovePicker picker(
        pos, moves, tt_move, pv_move, ply, pos.st->active_color, ctx.heuristics, eval, search
    );

    int32_t best = -search.inf;
    Move best_move = Move::none();
    int move_index = 0;

    Move move;
    while (picker.next(move)) {
        move_index++;

        State st;
        pos.do_move(move, st);

        int32_t score;
        if (move_index == 1) {
            score = -negamax(depth - 1, ply + 1, -beta, -alpha, ctx, false, eval, search);
        } else {
            const bool gives_check = pos.st->checkers != 0;

            const int reduction =
                !in_check && !gives_check && is_quiet(move) ? lmr_reduction(depth, move_index) : 0;
            score = -negamax(
                depth - 1 - reduction, ply + 1, -alpha - 1, -alpha, ctx, false, eval, search
            );
            if (reduction > 0 && score > alpha) {
                score = -negamax(depth - 1, ply + 1, -alpha - 1, -alpha, ctx, false, eval, search);
            }
            if (score > alpha && score < beta) {
                score = -negamax(depth - 1, ply + 1, -beta, -alpha, ctx, false, eval, search);
            }
        }

        pos.undo_move(move);

        if (ctx.stopped) return 0;

        if (score > best) {
            best = score;
            best_move = move;
        }

        if (score > alpha) {
            alpha = score;
            update_pv(ctx, ply, move);
        }

        if (alpha >= beta) {
            if (is_quiet(move) && ply < search.max_killer_ply) {
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
    } else if (best >= og_beta) {
        bound = TTBound::lower;
    } else {
        bound = TTBound::exact;
    }
    tt.set(
        pos.st->hash,
        TTentry{
            .key = pos.st->hash,
            .depth = static_cast<int16_t>(depth),
            .score = score_to_tt(best, ply, search),
            .bound = bound,
            .best_move = best_move
        }
    );

    return best;
}

int32_t chess::BetterEvalEngine::root_search(
    MoveStack<> &root_moves, uint16_t depth, SearchContext &ctx, int32_t alpha, int32_t beta,
    Move &best_move, const EvalSettings &eval, const SearchSettings &search
) {
    int32_t best_score = -search.inf;

    Move iteration_best = Move::none();
    Move pv_move = Move::none();
    if (ctx.prev_pv_length > 0) pv_move = ctx.prev_pv[0];

    Move tt_move = get_tt_move(ctx);

    MovePicker picker(
        pos, root_moves, tt_move, pv_move, 0, pos.st->active_color, ctx.heuristics, eval, search
    );

    Move move;
    while (picker.next(move)) {
        if (Clock::now() >= *ctx.deadline) {
            ctx.stopped = true;
            return 0;
        }

        State st;
        pos.do_move(move, st);
        const int32_t score = -negamax(depth - 1, 1, -beta, -alpha, ctx, false, eval, search);
        pos.undo_move(move);

        if (ctx.stopped) return 0;

        if (score > best_score) {
            best_score = score;
            iteration_best = move;
        }

        if (score > alpha) {
            alpha = score;

            ctx.pv[0][0] = move;
            for (int i = 0; i < ctx.pv_length[1]; ++i) ctx.pv[0][i + 1] = ctx.pv[1][i];
            ctx.pv_length[0] = ctx.pv_length[1] + 1;
        }

        if (alpha >= beta) break;
    }

    best_move = iteration_best;
    return best_score;
}

chess::BetterEvalEngine::SearchResult chess::BetterEvalEngine::search(
    std::chrono::milliseconds time, const EvalSettings &eval, const SearchSettings &search
) {
    SearchContext ctx{.deadline = Clock::now() + time, .stopped = false, .nodes = 0};

    SearchResult result{.best_move = Move::none(), .score = 0, .depth = 0};

    auto root_moves = chess::gen_moves<GenType::legal>(pos);
    if (pos.get_game_end(root_moves) != GameEnd::proceed) { return result; }
    result.best_move = root_moves[0];
    result.score = evaluate(eval);
    for (uint16_t depth = 1; depth < std::numeric_limits<uint16_t>().max(); depth++) {
        if (Clock::now() >= *ctx.deadline) break;

        ctx.stopped = false;

        ctx.prev_pv_length = ctx.pv_length[0];
        for (int i = 0; i < ctx.prev_pv_length; ++i) ctx.prev_pv[i] = ctx.pv[0][i];
        ctx.pv_length.fill(0);

        int32_t delta = search.aspiration_window;

        int32_t alpha = depth > 1 ? std::max(-search.inf, result.score - delta) : -search.inf;
        int32_t beta = depth > 1 ? std::min(search.inf, result.score + delta) : search.inf;

        int32_t iteration_score = -search.inf;
        Move iteration_best = result.best_move;
        while (true) {
            ctx.pv_length.fill(0);
            iteration_best = result.best_move;

            iteration_score =
                root_search(root_moves, depth, ctx, alpha, beta, iteration_best, eval, search);

            if (ctx.stopped) break;

            if (iteration_score <= alpha) {
                beta = alpha + (beta - alpha) / 2;
                alpha = std::max(-search.inf, iteration_score - delta);
            } else if (iteration_score >= beta) {
                beta = std::min(search.inf, iteration_score + delta);
            } else {
                break;
            }

            delta = delta >= search.inf / 2 ? search.inf : delta * 2;
        }

        if (ctx.stopped) break;

        result.best_move = iteration_best;
        result.score = iteration_score;
        result.depth = depth;
        result.nodes = ctx.nodes;
    }

    return result;
}

chess::BetterEvalEngine::SearchResult
chess::BetterEvalEngine::search(std::chrono::milliseconds time) {
    static constexpr EvalSettings eval_defaults{};
    static constexpr SearchSettings search_defaults{};
    return search(time, eval_defaults, search_defaults);
}

chess::BetterEvalEngine::SearchResult chess::BetterEvalEngine::search(
    uint16_t depth, const EvalSettings &eval, const SearchSettings &search
) {
    SearchContext ctx{.deadline{}, .stopped = false, .nodes = 0};

    SearchResult result{.best_move = Move::none(), .score = 0, .depth = 0};
    auto root_moves = chess::gen_moves<GenType::legal>(pos);
    if (pos.get_game_end(root_moves) != GameEnd::proceed) { return result; }

    ctx.pv_length.fill(0);

    result.score = root_search(
        root_moves, depth, ctx, -search.inf, search.inf, result.best_move, eval, search
    );
    result.depth = depth;
    result.nodes = ctx.nodes;

    return result;
}
chess::BetterEvalEngine::SearchResult chess::BetterEvalEngine::search(uint16_t depth) {
    static constexpr EvalSettings eval_defaults{};
    static constexpr SearchSettings search_defaults{};
    return search(depth, eval_defaults, search_defaults);
}
