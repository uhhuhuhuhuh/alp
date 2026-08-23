#include "genmoves.hpp"
#include "bitboard.hpp"
#include "constbb.hpp"
#include "move.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "square.hpp"
#include <bit>
#include <cassert>

// returns index of least significant bit.
static inline std::size_t poplsb(chess::bb::U64 &x) {
    int index = std::countr_zero(x);
    x &= x - 1;
    return index;
}

chess::bb::U64 chess::attacks_to_sqr(const Position &pos, sqr_t sqr, Color color) {
    Color enemy = !color;

    bb::U64 occupancy = pos.bb.get_occupancy();
    // Rook & Queen, Bishop & Queen
    bb::U64 RQ, BQ;
    RQ = BQ = pos.bb.get_queens()[enemy];
    RQ |= pos.bb.get_rooks()[enemy];
    BQ |= pos.bb.get_bishops()[enemy];

    return (bb::pawn_attacks[color][sqr] & pos.bb.get_pawns()[enemy]) |
           (bb::knight_attacks[sqr] & pos.bb.get_knights()[enemy]) |
           (bb::get_bishop_attack(sqr, occupancy) & BQ) |
           (bb::get_rook_attack(sqr, occupancy) & RQ) |
           (bb::king_attacks[sqr] & pos.bb.get_kings()[enemy]);
}

chess::PinBBs chess::get_pins(const Position &pos, Color color) {
    PinBBs pin_bbs;
    bb::U64 occ = pos.bb.get_occupancy();
    bb::U64 color_occ = pos.bb.get_every_type()[color];
    bb::U64 king_bb = pos.bb.get_kings()[color];
    assert(std::popcount(king_bb) == 1);
    sqr_t king_sqr = std::countr_zero(king_bb);

    bb::U64 queen_bb = pos.bb.get_queens()[!color];

    bb::U64 pinner = bb::xray_bishop_attack(occ, color_occ, king_sqr) &
                     (pos.bb.get_bishops()[!color] | queen_bb);
    pin_bbs.pinners |= pinner;
    while (pinner != 0) {
        sqr_t sqr = poplsb(pinner);
        bb::U64 between = bb::slider_between[sqr][king_sqr];
        sqr_t pinned_sqr = std::countr_zero(between & color_occ);
        pin_bbs.pin_rays[pinned_sqr] = between | (1ULL << sqr);
        pin_bbs.pinned |= 1ULL << pinned_sqr;
    }

    pinner =
        bb::xray_rook_attack(occ, color_occ, king_sqr) & (pos.bb.get_rooks()[!color] | queen_bb);
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
static inline bool is_self_check(Position &pos, const Move &move, Color color) {
    State st;
    do_move(pos, move, st);
    bb::U64 attackers = attacks_to_sqr(pos, std::countr_zero(pos.bb.get_kings()[color]), color);
    undo_move(pos, move);

    if (attackers != 0) {
        return true;
    }

    return false;
}

static bool is_capture_pawn_move_legal(Position &pos, Move &move) {
    const Piece &from_piece = pos.board[move.from], to_piece = pos.board[move.to];
    assert(from_piece.type == PieceType::pawn);

    move.flags = MoveFlag::capture;

    bool is_white = from_piece.color == Color::white;
    rank_t rank_sign = 1;
    if (!is_white) {
        rank_sign = -1;
    }

    if (to_piece.type != PieceType::none && to_piece.color == from_piece.color) {
        return false;
    }

    sqr_t en_passant_sqr = move.to + (rank_sign * -8);
    if (to_piece.type == PieceType::none) {
        if (en_passant_sqr != pos.st->en_passant_sqr) {
            return false;
        }
        move.flags = MoveFlag::ep;
        return true;
    }

    bb::U64 to_bb = 1ULL << move.to;
    if (((to_bb & bb::ranks[7]) != 0 && is_white) || ((to_bb & bb::ranks[0]) != 0 && !is_white)) {
        move.flags = MoveFlag::promotion;
    }

    return true;
}

static bool is_king_move_legal(Position &pos, Move &move) {
    if (move.to > 63 || move.from > 63 || move.to < 0 || move.from < 0) {
        return false;
    }

    const Piece &from_piece = pos.board[move.from], to_piece = pos.board[move.to];
    assert(from_piece.type == PieceType::king);

    if (to_piece.type != PieceType::none && to_piece.color == from_piece.color) {
        return false;
    }

    rank_t from_rank = move.from / 8, to_rank = move.to / 8;
    file_t from_file = move.from % 8, to_file = move.to % 8;

    file_t file_dist = abs(from_file - to_file);
    rank_t rank_dist = abs(from_rank - to_rank);

    if (rank_dist > 1 || file_dist > 2) {
        return false;
    }

    if (file_dist < 2) {
        if (pos.board[move.to].type != PieceType::none) {
            move.flags = capture;
        }
        return true;
    }

    if (pos.st->checks_bb != 0) {
        return false;
    }

    const bool is_white = from_piece.color == Color::white, queen_side = to_file < from_file;
    CastleRights rights = queen_side
                              ? (is_white ? CastleRights::white_queen : CastleRights::black_queen)
                              : (is_white ? CastleRights::white_king : CastleRights::black_king);
    if ((pos.st->castle_rights & rights) == 0) {
        return false;
    }

    sqr_t rook_sqr = (queen_side ? 0 : 7) + (from_rank * 8);
    assert(
        move.from == (is_white ? e1 : e8) && pos.board[rook_sqr].type == PieceType::rook &&
        pos.board[rook_sqr].color == from_piece.color
    );

    sqr_t step = queen_side ? -1 : 1;
    bool passed_to = false;
    for (sqr_t sqr = move.from + step; sqr != rook_sqr; sqr += step) {
        if (pos.board[sqr].type != PieceType::none) {
            return false;
        }
        if (sqr == move.to) {
            passed_to = true;
        }
        if (!passed_to && attacks_to_sqr(pos, sqr, from_piece.color) != 0) {
            return false;
        }
    }

    move.flags = MoveFlag::castle;
    return true;
}

inline static void push_king_move(Position &pos, sqr_t from, sqr_t to, std::vector<Move> &moves) {
    Move move(from, to);

    if (is_king_move_legal(pos, move)) {
        moves.push_back(move);
    }
}
} // namespace chess
void chess::gen_pawn_moves(Position &pos, sqr_t sqr, std::vector<Move> &moves) {
    assert(sqr >= 0 && sqr < 64);

    const Piece &piece = pos.board[sqr];
    assert(piece.type == PieceType::pawn);
    Color color = piece.color;

    rank_t rank_sign = 1, promotion = 7, starting = 1;
    if (piece.color == Color::black) {
        rank_sign = -1, promotion = 0, starting = 6;
    }

    bb::U64 occ = pos.bb.get_occupancy();
    bb::U64 sqr_bb = 1ULL << sqr;

    sqr_t single_push = sqr + (8 * rank_sign);
    bool is_single_push_empty = ((1ULL << single_push) & occ) == 0;
    Move move(sqr, single_push);
    if (is_single_push_empty) {
        // single push
        if (((1ULL << single_push) & bb::ranks[promotion]) != 0) {
            move.flags = MoveFlag::promotion;
        }
        moves.push_back(move);

        // double push
        move.to = sqr + (16 * rank_sign), move.flags = MoveFlag::quiet;
        if ((sqr_bb & bb::ranks[starting]) != 0 && ((1ULL << move.to) & occ) == 0) {
            moves.push_back(move);
        }
    }

    bb::U64 attacks = bb::pawn_attacks[color][sqr];
    while (attacks != 0) {
        move.to = poplsb(attacks);
        if (is_capture_pawn_move_legal(pos, move)) {
            moves.push_back(move);
        }
    }
}

void chess::gen_knight_moves(Position &pos, sqr_t sqr, std::vector<Move> &moves) {
    assert(sqr >= 0 && sqr < 64);
    assert(pos.board[sqr].type == PieceType::knight);

    Color color = pos.board[sqr].color;
    bb::U64 attack_bb = bb::knight_attacks[sqr] & ~(pos.bb.get_every_type()[color]);
    while (attack_bb != 0) {
        sqr_t to = poplsb(attack_bb);
        Move move(sqr, to);
        if (pos.board[to].type != PieceType::none) {
            move.flags = MoveFlag::capture;
        }

        moves.push_back(move);
    }
}

void chess::gen_bishop_moves(Position &pos, sqr_t sqr, std::vector<Move> &moves) {
    assert(sqr >= 0 && sqr < 64);
    assert(pos.board[sqr].type == PieceType::bishop);

    Color color = pos.board[sqr].color;
    bb::U64 attack_bb =
        bb::get_bishop_attack(sqr, pos.bb.get_occupancy()) & ~(pos.bb.get_every_type()[color]);
    while (attack_bb != 0) {
        sqr_t to = poplsb(attack_bb);
        Move move(sqr, to);
        if (pos.board[to].type != PieceType::none) {
            move.flags = MoveFlag::capture;
        }

        moves.push_back(move);
    }
}

void chess::gen_rook_moves(Position &pos, sqr_t sqr, std::vector<Move> &moves) {
    assert(sqr >= 0 && sqr < 64);
    assert(pos.board[sqr].type == PieceType::rook);

    Color color = pos.board[sqr].color;
    bb::U64 attack_bb =
        bb::get_rook_attack(sqr, pos.bb.get_occupancy()) & ~(pos.bb.get_every_type()[color]);
    while (attack_bb != 0) {
        sqr_t to = poplsb(attack_bb);
        Move move(sqr, to);
        if (pos.board[to].type != PieceType::none) {
            move.flags = MoveFlag::capture;
        }

        moves.push_back(move);
    }
}

void chess::gen_queen_moves(Position &pos, sqr_t sqr, std::vector<Move> &moves) {
    assert(sqr >= 0 && sqr < 64);
    assert(pos.board[sqr].type == PieceType::queen);

    bb::U64 occ = pos.bb.get_occupancy();
    bb::U64 attack_bb = bb::get_rook_attack(sqr, occ) | bb::get_bishop_attack(sqr, occ);
    attack_bb &= ~(pos.bb.get_every_type()[pos.board[sqr].color]);
    while (attack_bb != 0) {
        sqr_t to = poplsb(attack_bb);
        Move move(sqr, to);
        if (pos.board[to].type != PieceType::none) {
            move.flags = MoveFlag::capture;
        }

        moves.push_back(move);
    }
}

void chess::gen_king_moves(Position &pos, sqr_t sqr, std::vector<Move> &moves) {
    assert(sqr >= 0 && sqr < 64);
    assert(pos.board[sqr].type == PieceType::king);
    push_king_move(pos, sqr, sqr + 1, moves);
    push_king_move(pos, sqr, sqr - 1, moves);
    push_king_move(pos, sqr, sqr + 8, moves);
    push_king_move(pos, sqr, sqr - 8, moves);
    push_king_move(pos, sqr, sqr + 7, moves);
    push_king_move(pos, sqr, sqr - 7, moves);
    push_king_move(pos, sqr, sqr + 9, moves);
    push_king_move(pos, sqr, sqr - 9, moves);
    push_king_move(pos, sqr, sqr + 2, moves);
    push_king_move(pos, sqr, sqr - 2, moves);
}

void chess::gen_legal_moves(Position &pos, std::vector<Move> &legal) {
    std::vector<Move> moves;
    Color color = pos.st->active_color;
    PinBBs pin_bb = get_pins(pos, color);
    std::uint32_t checker_count = std::popcount(pos.st->checks_bb);
    bb::U64 king_bb = pos.bb.get_kings()[color];
    sqr_t king_sqr = std::countr_zero(king_bb);

    if (checker_count > 1) {
        assert(std::popcount(king_bb) == 1);
        gen_king_moves(pos, king_sqr, moves);
    } else [[likely]] {
        bb::U64 occ = pos.bb.get_every_type()[color];
        while (occ != 0) {
            sqr_t sqr = poplsb(occ);

            switch (pos.board[sqr].type) {
            case PieceType::pawn:   gen_pawn_moves(pos, sqr, moves); break;
            case PieceType::knight: gen_knight_moves(pos, sqr, moves); break;
            case PieceType::bishop: gen_bishop_moves(pos, sqr, moves); break;
            case PieceType::rook:   gen_rook_moves(pos, sqr, moves); break;
            case PieceType::queen:  gen_queen_moves(pos, sqr, moves); break;
            case PieceType::king:   gen_king_moves(pos, sqr, moves); break;
            default:                break;
            }
        }
    }

    for (const Move &move : moves) {
        bool is_pinned = ((1ULL << move.from) & pin_bb.pinned) != 0;
        if (checker_count == 1 && is_pinned) {
            continue;
        }

        PieceType type = pos.board[move.from].type;
        if (type == PieceType::king) {
            if (!is_self_check(pos, move, color)) {
                legal.push_back(move);
            }
            continue;
        }

        bb::U64 to_bb = 1ULL << move.to;
        if (checker_count == 1) {
            bool capture_checker = (to_bb & pos.st->checks_bb) != 0;

            bb::U64 ep_bb = 0ULL;
            if (pos.st->en_passant_sqr != -1) {
                ep_bb = 1ULL << (pos.st->en_passant_sqr + (color == Color::white ? 8 : -8));
            }
            bool ep_checker = type == PieceType::pawn && ((to_bb & ep_bb) != 0);

            sqr_t checker_bb = std::countr_zero(pos.st->checks_bb);
            bool block_checker = (to_bb & bb::slider_between[checker_bb][king_sqr]) != 0;

            if (capture_checker || ep_checker || block_checker) {
                legal.push_back(move);
            }
            continue;
        }

        if (is_pinned) {
            if ((to_bb & pin_bb.pin_rays[move.from]) != 0) {
                legal.push_back(move);
            }
            continue;
        }

        legal.push_back(move);
    }
}