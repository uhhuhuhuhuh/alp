#include "genmoves.hpp"
#include "bitboard.hpp"
#include "constbb.hpp"
#include "move.hpp"
#include "piece_color.hpp"
#include "position.hpp"
#include "square.hpp"

namespace chess {

static bool is_pseudo_non_king_move_legal(Position &pos, const Move &move, const LegalCtx &ctx) {
    const bool is_pinned = (bb::bb(move.from) & ctx.pin_bb.pinned) != 0;

    if (ctx.in_check && is_pinned) { return false; }

    const Color color = pos.board[move.from].color;
    const bb::U64 to_bb = bb::bb(move.to);

    if (ctx.in_check) {
        if (to_bb & ctx.evasion_mask) { return true; }

        if (move.flag == MoveFlag::ep) {
            const sqr_t captured_sqr = pos.st->ep_sqr - (color == Color::white ? 8 : -8);
            return bb::bb(captured_sqr) & pos.st->checkers;
        }
        return false;
    }

    if (is_pinned) { return (to_bb & ctx.pin_bb.pin_rays[move.from]) != 0; }

    if (move.flag == MoveFlag::ep) {
        const bb::U64 king_bb = pos.bb.get(PieceType::king)[color];
        if ((bb::ranks[move.from / 8] & king_bb) == 0) { return true; }

        const sqr_t captured_sqr = pos.st->ep_sqr - (color == Color::white ? 8 : -8);

        const Color enemy = !color;
        pos.bb.remove(PieceType::pawn, color, move.from);
        pos.bb.remove(PieceType::pawn, enemy, captured_sqr);

        const bb::U64 RQ = pos.bb.get(PieceType::rook)[enemy] | pos.bb.get(PieceType::queen)[enemy];
        const bb::U64 attackers = bb::get_rook_attack(lsb(king_bb), pos.bb.occupancy()) & RQ;

        pos.bb.place(PieceType::pawn, color, move.from);
        pos.bb.place(PieceType::pawn, enemy, captured_sqr);

        return attackers == 0;
    }

    return true;
}

static inline Piece do_move_bb(Position &pos, const Move &move) {
    PieceType type = pos.board[move.from].type;
    Color color = pos.board[move.from].color;

    switch (move.flag) {
    case MoveFlag::capture: {
        PieceType captured_type = pos.board[move.to].type;
        Color captured_color = pos.board[move.to].color;
        pos.bb.capture(type, color, move.from, captured_type, captured_color, move.to);
        return pos.board[move.to];
    }
    default: pos.bb.move(type, color, move.from, move.to); return Piece();
    }
}
static inline void undo_move_bb(Position &pos, const Move &move, const Piece &captured) {
    PieceType type = pos.board[move.from].type;
    Color color = pos.board[move.from].color;

    pos.bb.move(type, color, move.to, move.from);
    if (captured.type != PieceType::none) { pos.bb.place(captured.type, captured.color, move.to); }
}
static inline bool is_self_check(Position &pos, const Move &move, Color color) {
    Piece captured = do_move_bb(pos, move);
    bb::U64 attackers = pos.attacks_to_sqr(lsb(pos.bb.get(PieceType::king)[color]), color);
    undo_move_bb(pos, move, captured);

    return attackers != 0;
}

template <GenType gen_type, typename legal_fn>
static inline void push_move(
    Position &pos, sqr_t from, sqr_t to, uint8_t flags, PieceType promotion, MoveStack<> &moves,
    legal_fn is_legal
) {
    if constexpr (gen_type != GenType::legal) {
        moves.emplace(from, to, flags, promotion);
        return;
    }
    Move move(from, to, flags, promotion);
    if (is_legal(pos, move)) { moves.push(move); }
}

template <GenType gen_type>
static inline void
push_king_move(Position &pos, sqr_t from, sqr_t to, uint8_t flags, MoveStack<> &moves) {
    const Color color = pos.board[from].color;

    push_move<gen_type>(
        pos, from, to, flags, PieceType::none, moves, [color](Position &pos, Move &move) {
            return !is_self_check(pos, move, color);
        }
    );
}
template <GenType gen_type>
static inline void push_non_king_move(
    Position &pos, sqr_t from, sqr_t to, uint8_t flags, MoveStack<> &moves, const LegalCtx &ctx,
    PieceType promotion = PieceType::none
) {
    push_move<gen_type>(pos, from, to, flags, promotion, moves, [&](Position &pos, Move &move) {
        return is_pseudo_non_king_move_legal(pos, move, ctx);
    });
}
template <GenType gen_type>
static inline void push_promotions(
    Position &pos, sqr_t from, sqr_t to, uint8_t flags, MoveStack<> &moves, const LegalCtx &ctx
) {
    constexpr std::array<PieceType, 4> promotion_types = {
        PieceType::knight, PieceType::bishop, PieceType::rook, PieceType::queen
    };
    for (PieceType type : promotion_types) {
        push_non_king_move<gen_type>(pos, from, to, flags, moves, ctx, type);
    }
}

template <GenType gen_type>
static void push_king_castle(Position &pos, sqr_t from, sqr_t to, MoveStack<> &moves) {
    if (to > 63 || from > 63 || to < 0 || from < 0) { return; }

    const Piece &from_piece = pos.board[from], to_piece = pos.board[to];
    assert(from_piece.type == PieceType::king);

    rank_t from_rank = from / 8, to_rank = to / 8;
    file_t from_file = from % 8, to_file = to % 8;
    file_t file_dist = abs(from_file - to_file);
    rank_t rank_dist = abs(from_rank - to_rank);

    if (rank_dist > 1 || file_dist > 2) { return; }

    if (pos.st->checkers != 0) { return; }

    const bool is_white = from_piece.color == Color::white, queen_side = to_file < from_file;
    CastleRights rights = queen_side
                              ? (is_white ? CastleRights::white_queen : CastleRights::black_queen)
                              : (is_white ? CastleRights::white_king : CastleRights::black_king);
    if ((pos.st->castle_rights & rights) == 0) { return; }

    sqr_t rook_sqr = (queen_side ? 0 : 7) + (from_rank * 8);
    assert(
        from == (is_white ? e1 : e8) && pos.board[rook_sqr].type == PieceType::rook &&
        pos.board[rook_sqr].color == from_piece.color
    );

    sqr_t step = queen_side ? -1 : 1;
    bool passed_to = false;
    for (sqr_t sqr = from + step; sqr != rook_sqr; sqr += step) {
        if (pos.board[sqr].type != PieceType::none) { return; }
        if (sqr == to) { passed_to = true; }
        if (!passed_to && pos.attacks_to_sqr(sqr, from_piece.color) != 0) { return; }
    }

    push_king_move<gen_type>(pos, from, to, MoveFlag::castle, moves);
}

template <chess::GenType gen_type>
static inline void
gen_attacks(Position &pos, sqr_t sqr, MoveStack<> &moves, const LegalCtx &ctx, bb::U64 attacks) {
    Color color = pos.board[sqr].color;
    bb::U64 attack_bb = attacks & ~(pos.bb.every_type()[color]);
    while (attack_bb != 0) {
        sqr_t to = poplsb(attack_bb);
        uint8_t flag = pos.board[to].type == PieceType::none ? MoveFlag::quiet : MoveFlag::capture;
        push_non_king_move<gen_type>(pos, sqr, to, flag, moves, ctx);
    }
}
} // namespace chess

template <chess::GenType gen_type>
void chess::gen_pawn_moves_bulk(Position &pos, MoveStack<> &moves, const LegalCtx &ctx) {
    Color color = pos.st->active_color;
    const bb::U64 occ = pos.bb.occupancy();
    const bb::U64 empty = ~occ;
    const bb::U64 enemy = pos.bb.every_type()[!color];

    const bb::U64 pawns = pos.bb.get(PieceType::pawn)[color];

    const bool white = color == Color::white;

    const bb::U64 promotion_rank = white ? bb::ranks[Rank::R8] : bb::ranks[Rank::R1];
    const bb::U64 starting_rank = white ? bb::ranks[Rank::R2] : bb::ranks[Rank::R7];

    bb::U64 single = (white ? pawns << 8 : pawns >> 8) & empty;

    bb::U64 push_quiets = single & ~promotion_rank;
    while (push_quiets != 0) {
        const sqr_t to = poplsb(push_quiets);
        const sqr_t from = to + (white ? -8 : 8);
        push_non_king_move<gen_type>(pos, from, to, MoveFlag::quiet, moves, ctx);
    }
    bb::U64 push_promos = single & promotion_rank;
    while (push_promos != 0) {
        const sqr_t to = poplsb(push_promos);
        const sqr_t from = to + (white ? -8 : 8);
        push_promotions<gen_type>(pos, from, to, MoveFlag::quiet, moves, ctx);
    }

    bb::U64 double_pushes;
    if (white) {
        const bb::U64 one_from_start = single & (starting_rank << 8);
        double_pushes = (one_from_start << 8) & empty;
    } else {
        const bb::U64 one_from_start = single & (starting_rank >> 8);
        double_pushes = (one_from_start >> 8) & empty;
    }
    while (double_pushes) {
        const sqr_t to = poplsb(double_pushes);
        const sqr_t from = to + (white ? -16 : 16);
        push_non_king_move<gen_type>(pos, from, to, MoveFlag::ep_victim, moves, ctx);
    }

    bb::U64 captures_left;
    bb::U64 captures_right;
    if (white) {
        captures_left = (pawns & ~bb::files[File::A]) << 7;
        captures_right = (pawns & ~bb::files[File::H]) << 9;
    } else {
        captures_left = (pawns & ~bb::files[File::H]) >> 7;
        captures_right = (pawns & ~bb::files[File::A]) >> 9;
    }
    bb::U64 captures = captures_left | captures_right;
    captures &= enemy;

    bb::U64 capture_quiets = captures & ~promotion_rank;
    while (capture_quiets) {
        const sqr_t to = poplsb(capture_quiets);
        // Recover the source square from the reverse pawn attacks.
        bb::U64 sources = bb::pawn_attacks[!color][to] & pawns;
        while (sources) {
            const sqr_t from = poplsb(sources);
            push_non_king_move<gen_type>(pos, from, to, MoveFlag::capture, moves, ctx);
        }
    }

    bb::U64 capture_promos = captures & promotion_rank;
    while (capture_promos) {
        const sqr_t to = poplsb(capture_promos);
        bb::U64 sources = bb::pawn_attacks[!color][to] & pawns;
        while (sources) {
            const sqr_t from = poplsb(sources);
            push_promotions<gen_type>(pos, from, to, MoveFlag::capture, moves, ctx);
        }
    }

    const sqr_t ep_sqr = pos.st->ep_sqr;
    if (ep_sqr != State::ep_none) {
        const bb::U64 ep_bb = bb::bb(ep_sqr);
        bb::U64 ep_sources = bb::pawn_attacks[!color][ep_sqr] & pawns;
        while (ep_sources) {
            const sqr_t from = poplsb(ep_sources);
            push_non_king_move<gen_type>(pos, from, ep_sqr, MoveFlag::ep, moves, ctx);
        }
    }
}

template <chess::GenType gen_type>
void chess::gen_knight_moves(Position &pos, sqr_t sqr, MoveStack<> &moves, const LegalCtx &ctx) {
    assert(sqr >= 0 && sqr < 64);
    assert(pos.board[sqr].type == PieceType::knight);

    gen_attacks<gen_type>(pos, sqr, moves, ctx, bb::knight_attacks[sqr]);
}

template <chess::GenType gen_type>
void chess::gen_bishop_moves(Position &pos, sqr_t sqr, MoveStack<> &moves, const LegalCtx &ctx) {
    assert(sqr >= 0 && sqr < 64);
    assert(pos.board[sqr].type == PieceType::bishop);

    gen_attacks<gen_type>(pos, sqr, moves, ctx, bb::get_bishop_attack(sqr, pos.bb.occupancy()));
}

template <chess::GenType gen_type>
void chess::gen_rook_moves(Position &pos, sqr_t sqr, MoveStack<> &moves, const LegalCtx &ctx) {
    assert(sqr >= 0 && sqr < 64);
    assert(pos.board[sqr].type == PieceType::rook);

    gen_attacks<gen_type>(pos, sqr, moves, ctx, bb::get_rook_attack(sqr, pos.bb.occupancy()));
}

template <chess::GenType gen_type>
void chess::gen_queen_moves(Position &pos, sqr_t sqr, MoveStack<> &moves, const LegalCtx &ctx) {
    assert(sqr >= 0 && sqr < 64);
    assert(pos.board[sqr].type == PieceType::queen);

    bb::U64 occ = pos.bb.occupancy();
    gen_attacks<gen_type>(
        pos, sqr, moves, ctx, bb::get_rook_attack(sqr, occ) | bb::get_bishop_attack(sqr, occ)
    );
}

template <chess::GenType gen_type>
void chess::gen_king_moves(Position &pos, sqr_t sqr, MoveStack<> &moves) {
    assert(sqr >= 0 && sqr < 64);
    assert(pos.board[sqr].type == PieceType::king);

    Color color = pos.board[sqr].color;
    bb::U64 attack_bb = bb::king_attacks[sqr] & ~(pos.bb.every_type()[color]);
    while (attack_bb != 0) {
        sqr_t to = poplsb(attack_bb);
        uint8_t flag = pos.board[to].type == PieceType::none ? MoveFlag::quiet : MoveFlag::capture;
        push_king_move<gen_type>(pos, sqr, to, flag, moves);
    }
    push_king_castle<gen_type>(pos, sqr, sqr + 2, moves);
    push_king_castle<gen_type>(pos, sqr, sqr - 2, moves);
}

template <chess::GenType gen_type> chess::MoveStack<> chess::gen_moves(Position &pos) {
    MoveStack<> moves;

    Color color = pos.st->active_color;

    bb::U64 king_bb = pos.bb.get(PieceType::king)[color];
    assert(popcount(king_bb) == 1); // this assertion failes
    sqr_t king_sqr = lsb(king_bb);

    std::uint32_t checker_count = 0;
    if constexpr (gen_type == GenType::legal) {
        pos.st->checkers = pos.attacks_to_sqr(king_sqr, color);
        checker_count = popcount(pos.st->checkers);
    }

    if (checker_count > 1) {
        gen_king_moves<gen_type>(pos, king_sqr, moves);
        return moves;
    }

    PinBB pin_bb = pos.get_pins(color);

    const bb::U64 checkers = pos.st->checkers;
    const bool in_check = checker_count != 0;
    const sqr_t checker_sqr = in_check ? lsb(checkers) : 0;

    const bb::U64 evasion_mask =
        in_check ? checkers | bb::slider_between[checker_sqr][king_sqr] : 0;

    LegalCtx ctx{
        .pin_bb = pin_bb,
        .king_sqr = king_sqr,
        .checker_sqr = checker_sqr,
        .evasion_mask = evasion_mask,
        .in_check = in_check
    };

    gen_pawn_moves_bulk<gen_type>(pos, moves, ctx);

    bb::U64 occ = pos.bb.every_type()[color] & ~pos.bb.get(PieceType::pawn)[color];
    while (occ != 0) {
        sqr_t sqr = poplsb(occ);

        switch (pos.board[sqr].type) {
        case PieceType::knight: gen_knight_moves<gen_type>(pos, sqr, moves, ctx); break;
        case PieceType::bishop: gen_bishop_moves<gen_type>(pos, sqr, moves, ctx); break;
        case PieceType::rook:   gen_rook_moves<gen_type>(pos, sqr, moves, ctx); break;
        case PieceType::queen:  gen_queen_moves<gen_type>(pos, sqr, moves, ctx); break;
        case PieceType::king:   gen_king_moves<gen_type>(pos, sqr, moves); break;
        default:                break;
        }
    }
    return moves;
}

template chess::MoveStack<> chess::gen_moves<chess::GenType::legal>(Position &pos);
template chess::MoveStack<> chess::gen_moves<chess::GenType::pseudo>(Position &pos);
