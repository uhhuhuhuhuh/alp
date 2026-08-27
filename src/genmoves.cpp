#include "genmoves.hpp"
#include "bitboard.hpp"
#include "constbb.hpp"
#include "move.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "square.hpp"
#include <bit>
#include <cassert>
#include <cstdint>

// returns index of least significant bit.
static inline std::size_t poplsb(chess::bb::U64 &x) {
    int index = std::countr_zero(x);
    x &= x - 1;
    return index;
}

chess::bb::U64 chess::attacks_to_sqr(const Position &pos, sqr_t sqr, Color color) {
    Color enemy = !color;

    bb::U64 occupancy = pos.bb.occupancy();
    // Rook & Queen, Bishop & Queen
    bb::U64 RQ, BQ;
    RQ = BQ = pos.bb.get(PieceType::queen)[enemy];
    RQ |= pos.bb.get(PieceType::rook)[enemy];
    BQ |= pos.bb.get(PieceType::bishop)[enemy];

    return (bb::pawn_attacks[color][sqr] & pos.bb.get(PieceType::pawn)[enemy]) |
           (bb::knight_attacks[sqr] & pos.bb.get(PieceType::knight)[enemy]) |
           (bb::get_bishop_attack(sqr, occupancy) & BQ) |
           (bb::get_rook_attack(sqr, occupancy) & RQ) |
           (bb::king_attacks[sqr] & pos.bb.get(PieceType::king)[enemy]);
}

chess::PinBBs chess::get_pins(const Position &pos, Color color) {
    PinBBs pin_bbs;
    bb::U64 occ = pos.bb.occupancy();
    bb::U64 color_occ = pos.bb.every_type()[color];
    bb::U64 king_bb = pos.bb.get(PieceType::king)[color];
    assert(std::popcount(king_bb) == 1);
    sqr_t king_sqr = std::countr_zero(king_bb);

    bb::U64 queen_bb = pos.bb.get(PieceType::queen)[!color];

    bb::U64 pinner = bb::xray_bishop_attack(occ, color_occ, king_sqr) &
                     (pos.bb.get(PieceType::bishop)[!color] | queen_bb);
    pin_bbs.pinners |= pinner;
    while (pinner != 0) {
        sqr_t sqr = poplsb(pinner);
        bb::U64 between = bb::slider_between[sqr][king_sqr];
        sqr_t pinned_sqr = std::countr_zero(between & color_occ);
        pin_bbs.pin_rays[pinned_sqr] = between | (1ULL << sqr);
        pin_bbs.pinned |= 1ULL << pinned_sqr;
    }

    pinner = bb::xray_rook_attack(occ, color_occ, king_sqr) &
             (pos.bb.get(PieceType::rook)[!color] | queen_bb);
    pin_bbs.pinners |= pinner;
    while (pinner != 0) {
        sqr_t sqr = poplsb(pinner);
        bb::U64 between = bb::slider_between[sqr][king_sqr];
        sqr_t pinned_sqr = std::countr_zero(between & color_occ);
        pin_bbs.pin_rays[pinned_sqr] = between | (1ULL << sqr);
        pin_bbs.pinned |= 1ULL << pinned_sqr;
    }

    return pin_bbs;
}

namespace chess {
static bool is_pseudo_move_non_king_legal(Position &pos, const Move &move, const PinBBs &pin_bb) {
    bool is_pinned = ((1ULL << move.from) & pin_bb.pinned) != 0;
    std::uint32_t checker_count = std::popcount(pos.st->checks_bb);

    if (std::popcount(pos.st->checks_bb) == 1 && is_pinned) {
        return false;
    }

    PieceType type = pos.board[move.from].type;
    Color color = pos.board[move.from].color;
    bb::U64 to_bb = 1ULL << move.to;
    if (checker_count == 1) {
        bool capture_checker = (to_bb & pos.st->checks_bb) != 0;

        bb::U64 ep_bb = 0ULL;
        if (pos.st->ep_sqr != -1) {
            ep_bb = 1ULL << (pos.st->ep_sqr + (color == Color::white ? 8 : -8));
        }
        bool ep_checker = type == PieceType::pawn && ((to_bb & ep_bb) != 0);

        sqr_t checker_bb = std::countr_zero(pos.st->checks_bb);
        sqr_t king_sqr = std::countr_zero(pos.bb.get(PieceType::king)[color]);
        bool block_checker = (to_bb & bb::slider_between[checker_bb][king_sqr]) != 0;

        if (capture_checker || ep_checker || block_checker) {
            return true;
        }
        return false;
    }

    if (is_pinned) {
        if (((1ULL << move.to) & pin_bb.pin_rays[move.from]) != 0) {
            return true;
        }
        return false;
    }

    return true;
}

static inline Piece do_move_bb(Position &pos, const Move &move) {
    PieceType type = pos.board[move.from].type;
    Color color = pos.board[move.from].color;

    switch (move.flags) {
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
    if (captured.type != PieceType::none) {
        pos.bb.place(captured.type, captured.color, move.to);
    }
}
static inline bool is_self_check(Position &pos, const Move &move, Color color) {
    State st;
    Piece captured = do_move_bb(pos, move);
    bb::U64 attackers =
        attacks_to_sqr(pos, std::countr_zero(pos.bb.get(PieceType::king)[color]), color);
    undo_move_bb(pos, move, captured);

    if (attackers != 0) {
        return true;
    }

    return false;
}

template <GenType gen_type, typename legal_fn>
static inline void
push_move(Position &pos, sqr_t from, sqr_t to, uint8_t flags, MoveArray &moves, legal_fn is_legal) {
    if constexpr (gen_type == GenType::legal) {
        Move move(from, to, flags);
        if (is_legal(pos, move)) {
            moves.push_back(move);
        }
        return;
    }

    moves.emplace_back(from, to, flags);
}

template <GenType gen_type>
static inline void
push_king_move(Position &pos, sqr_t from, sqr_t to, uint8_t flags, MoveArray &moves) {
    Color color = pos.board[from].color;

    push_move<gen_type>(pos, from, to, flags, moves, [color](Position &pos, Move &move) {
        return !is_self_check(pos, move, color);
    });
}
template <GenType gen_type>
static inline void push_non_king_move(
    Position &pos, sqr_t from, sqr_t to, uint8_t flags, MoveArray &moves, const PinBBs &pins
) {
    push_move<gen_type>(pos, from, to, flags, moves, [&pins](Position &pos, Move &move) {
        return is_pseudo_move_non_king_legal(pos, move, pins);
    });
}

template <GenType gen_type>
static void push_king_castle(Position &pos, sqr_t from, sqr_t to, MoveArray &moves) {
    if (to > 63 || from > 63 || to < 0 || from < 0) {
        return;
    }

    const Piece &from_piece = pos.board[from], to_piece = pos.board[to];
    assert(from_piece.type == PieceType::king);

    if (to_piece.type != PieceType::none && to_piece.color == from_piece.color) {
        return;
    }

    rank_t from_rank = from / 8, to_rank = to / 8;
    file_t from_file = from % 8, to_file = to % 8;
    file_t file_dist = abs(from_file - to_file);
    rank_t rank_dist = abs(from_rank - to_rank);

    if (rank_dist > 1 || file_dist > 2) {
        return;
    }

    if (pos.st->checks_bb != 0) {
        return;
    }

    const bool is_white = from_piece.color == Color::white, queen_side = to_file < from_file;
    CastleRights rights = queen_side
                              ? (is_white ? CastleRights::white_queen : CastleRights::black_queen)
                              : (is_white ? CastleRights::white_king : CastleRights::black_king);
    if ((pos.st->castle_rights & rights) == 0) {
        return;
    }

    sqr_t rook_sqr = (queen_side ? 0 : 7) + (from_rank * 8);
    assert(
        from == (is_white ? e1 : e8) && pos.board[rook_sqr].type == PieceType::rook &&
        pos.board[rook_sqr].color == from_piece.color
    );

    sqr_t step = queen_side ? -1 : 1;
    bool passed_to = false;
    for (sqr_t sqr = from + step; sqr != rook_sqr; sqr += step) {
        if (pos.board[sqr].type != PieceType::none) {
            return;
        }
        if (sqr == to) {
            passed_to = true;
        }
        if (!passed_to && attacks_to_sqr(pos, sqr, from_piece.color) != 0) {
            return;
        }
    }

    push_king_move<gen_type>(pos, from, to, MoveFlag::castle, moves);
}
} // namespace chess

template <chess::GenType gen_type>
void chess::gen_pawn_moves(Position &pos, sqr_t sqr, MoveArray &moves, const PinBBs &pins) {
    assert(sqr >= 0 && sqr < 64);

    const Piece &piece = pos.board[sqr];
    assert(piece.type == PieceType::pawn);
    Color color = piece.color;

    sqr_t dir = 8, promotion = 7, starting = 1;
    if (piece.color == Color::black) {
        dir = -8, promotion = 0, starting = 6;
    }

    bb::U64 occ = pos.bb.occupancy();
    bb::U64 sqr_bb = 1ULL << sqr;

    sqr_t single_push = sqr + dir;
    bb::U64 single_push_bb = (1ULL << single_push);
    bool is_single_push_empty = (single_push_bb & occ) == 0;

    if (is_single_push_empty) {
        uint8_t flag =
            (single_push_bb & bb::ranks[promotion]) != 0 ? MoveFlag::promotion : MoveFlag::quiet;
        push_non_king_move<gen_type>(pos, sqr, single_push, flag, moves, pins);
    }
    sqr_t double_push = sqr + (2 * dir);
    if (is_single_push_empty && (sqr_bb & bb::ranks[starting]) != 0 &&
        ((1ULL << double_push) & occ) == 0) {
        push_non_king_move<gen_type>(pos, sqr, double_push, MoveFlag::ep_victim, moves, pins);
    }

    bb::U64 ep_bb = pos.st->ep_sqr == -1 ? 0ULL : (1ULL << (pos.st->ep_sqr + dir));
    bb::U64 attacks = bb::pawn_attacks[color][sqr] & (pos.bb.every_type()[!color] | ep_bb);
    while (attacks != 0) {
        sqr_t to = poplsb(attacks);
        uint8_t flag = pos.board[to].type != PieceType::none ? MoveFlag::capture : MoveFlag::ep;
        push_non_king_move<gen_type>(pos, sqr, to, flag, moves, pins);
    }
}

template <chess::GenType gen_type>
void chess::gen_knight_moves(Position &pos, sqr_t sqr, MoveArray &moves, const PinBBs &pins) {
    assert(sqr >= 0 && sqr < 64);
    assert(pos.board[sqr].type == PieceType::knight);

    Color color = pos.board[sqr].color;
    bb::U64 attack_bb = bb::knight_attacks[sqr] & ~(pos.bb.every_type()[color]);
    while (attack_bb != 0) {
        sqr_t to = poplsb(attack_bb);
        uint8_t flag = pos.board[to].type == PieceType::none ? MoveFlag::quiet : MoveFlag::capture;
        push_non_king_move<gen_type>(pos, sqr, to, flag, moves, pins);
    }
}

template <chess::GenType gen_type>
void chess::gen_bishop_moves(Position &pos, sqr_t sqr, MoveArray &moves, const PinBBs &pins) {
    assert(sqr >= 0 && sqr < 64);
    assert(pos.board[sqr].type == PieceType::bishop);

    Color color = pos.board[sqr].color;
    bb::U64 attack_bb =
        bb::get_bishop_attack(sqr, pos.bb.occupancy()) & ~(pos.bb.every_type()[color]);
    while (attack_bb != 0) {
        sqr_t to = poplsb(attack_bb);
        uint8_t flag = pos.board[to].type == PieceType::none ? MoveFlag::quiet : MoveFlag::capture;
        push_non_king_move<gen_type>(pos, sqr, to, flag, moves, pins);
    }
}

template <chess::GenType gen_type>
void chess::gen_rook_moves(Position &pos, sqr_t sqr, MoveArray &moves, const PinBBs &pins) {
    assert(sqr >= 0 && sqr < 64);
    assert(pos.board[sqr].type == PieceType::rook);

    Color color = pos.board[sqr].color;
    bb::U64 attack_bb =
        bb::get_rook_attack(sqr, pos.bb.occupancy()) & ~(pos.bb.every_type()[color]);
    while (attack_bb != 0) {
        sqr_t to = poplsb(attack_bb);
        uint8_t flag = pos.board[to].type == PieceType::none ? MoveFlag::quiet : MoveFlag::capture;
        push_non_king_move<gen_type>(pos, sqr, to, flag, moves, pins);
    }
}

template <chess::GenType gen_type>
void chess::gen_queen_moves(Position &pos, sqr_t sqr, MoveArray &moves, const PinBBs &pins) {
    assert(sqr >= 0 && sqr < 64);
    assert(pos.board[sqr].type == PieceType::queen);

    bb::U64 occ = pos.bb.occupancy();
    bb::U64 attack_bb = bb::get_rook_attack(sqr, occ) | bb::get_bishop_attack(sqr, occ);
    attack_bb &= ~(pos.bb.every_type()[pos.board[sqr].color]);
    while (attack_bb != 0) {
        sqr_t to = poplsb(attack_bb);
        uint8_t flag = pos.board[to].type == PieceType::none ? MoveFlag::quiet : MoveFlag::capture;
        push_non_king_move<gen_type>(pos, sqr, to, flag, moves, pins);
    }
}

template <chess::GenType gen_type>
void chess::gen_king_moves(Position &pos, sqr_t sqr, MoveArray &moves) {
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

template <chess::GenType gen_type> chess::MoveArray chess::gen_moves(Position &pos) {
    MoveArray moves;
    Color color = pos.st->active_color;
    PinBBs pin_bb = get_pins(pos, color);
    std::uint32_t checker_count = std::popcount(pos.st->checks_bb);
    bb::U64 king_bb = pos.bb.get(PieceType::king)[color];
    sqr_t king_sqr = std::countr_zero(king_bb);

    if (checker_count > 1) {
        assert(std::popcount(king_bb) == 1);
        gen_king_moves<gen_type>(pos, king_sqr, moves);
    } else [[likely]] {
        bb::U64 occ = pos.bb.every_type()[color];
        while (occ != 0) {
            sqr_t sqr = poplsb(occ);

            switch (pos.board[sqr].type) {
            case PieceType::pawn:   gen_pawn_moves<gen_type>(pos, sqr, moves, pin_bb); break;
            case PieceType::knight: gen_knight_moves<gen_type>(pos, sqr, moves, pin_bb); break;
            case PieceType::bishop: gen_bishop_moves<gen_type>(pos, sqr, moves, pin_bb); break;
            case PieceType::rook:   gen_rook_moves<gen_type>(pos, sqr, moves, pin_bb); break;
            case PieceType::queen:  gen_queen_moves<gen_type>(pos, sqr, moves, pin_bb); break;
            case PieceType::king:   gen_king_moves<gen_type>(pos, sqr, moves); break;
            default:                break;
            }
        }
    }

    return moves;
}

template chess::MoveArray chess::gen_moves<chess::GenType::legal>(Position &pos);
template chess::MoveArray chess::gen_moves<chess::GenType::pseudo>(Position &pos);