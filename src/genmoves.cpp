#include "genmoves.hpp"
#include "bitboard.hpp"
#include "constbb.hpp"
#include "piece.hpp"
#include "position.hpp"
#include "square.hpp"
#include <cassert>
#include <cmath>

// returns index of least significant bit.
static std::size_t poplsb(chess::bb::U64 &x) {
    int index = std::countr_zero(x);
    x &= x - 1;
    return index;
}

chess::bb::U64 chess::attack_bb_to_king(const Position &pos, Color color) {
    assert(std::popcount(pos.bb.get_kings()[color]) == 1);
    sqr_t sqr = std::countr_zero(pos.bb.get_kings()[color]);
    Color enemy = color == Color::white ? Color::black : Color::white;

    bb::U64 occupancy = pos.bb.get_occupancy();
    // Rook & Queen, Bishop & Queen
    bb::U64 RQ, BQ;
    RQ = BQ = pos.bb.get_queens()[enemy];
    RQ |= pos.bb.get_rooks()[enemy];
    BQ |= pos.bb.get_bishops()[enemy];

    return (bb::pawn_attacks[color][sqr] & pos.bb.get_pawns()[enemy]) |
           (bb::knight_attacks[sqr] & pos.bb.get_knights()[enemy]) |
           (bb::get_bishop_attack(sqr, occupancy) & BQ) |
           (bb::get_rook_attack(sqr, occupancy) & RQ);
}

namespace chess {
static bool is_pawn_move_legal(Position &pos, Move &move) {
    if (move.to > 63 || move.from > 63 || move.to < 0 || move.from < 0) {
        return false;
    }

    const Piece &from_piece = pos.board[move.from];
    assert(from_piece.type == PieceType::pawn);
    const Piece &to_piece = pos.board[move.to];

    bool is_from_white = from_piece.color == Color::white;
    rank_t rank_sign = 1;
    if (!is_from_white) {
        rank_sign = -1;
    }

    if (to_piece.type != PieceType::none && to_piece.color == from_piece.color) {
        return false;
    }

    rank_t from_rank = move.from / 8;
    file_t from_file = move.from % 8;
    rank_t to_rank = move.to / 8;
    file_t to_file = move.to % 8;
    if (is_from_white) {
        if (move.to <= move.from || to_rank <= from_rank || to_rank - 2 > from_rank) {
            return false;
        }
    } else if (move.to >= move.from || to_rank >= from_rank || to_rank + 2 < from_rank) {
        return false;
    }

    file_t file_dist = abs(from_file - to_file);
    rank_t rank_dist = abs(from_rank - to_rank);
    if (rank_dist > 2 || file_dist > 1) {
        return false;
    }
    if (file_dist == 1) {
        if (to_piece.type == PieceType::none && rank_dist == 1) {
            // TODO: add check for en passant
            return false;
        }
    } else {
        if (to_piece.type != PieceType::none) {
            return false;
        }
        if (rank_dist == 2) {
            if ((is_from_white && from_rank != 1) || (!is_from_white && from_rank != 6)) {
                return false;
            }
            if (pos.board[move.to - 8 * rank_sign].type != PieceType::none) {
                return false;
            }
        }
    }

    if ((to_rank == 7 && is_from_white) || (to_rank == 0 && !is_from_white)) {
        // TODO: add promotion
    }

    do_move(pos, move);
    bb::U64 attackers = attack_bb_to_king(pos, from_piece.color);
    undo_move(pos, move);

    if (attackers != 0) {
        return false;
    }

    return true;
}

static bool is_knight_move_legal(Position &pos, Move &move) {
    if (move.to > 63 || move.from > 63 || move.to < 0 || move.from < 0) {
        return false;
    }

    const Piece &from_piece = pos.board[move.from];
    assert(from_piece.type == PieceType::knight);
    const Piece &to_piece = pos.board[move.to];

    if (to_piece.type != PieceType::none && to_piece.color == from_piece.color) {
        return false;
    }

    rank_t from_rank = move.from / 8;
    file_t from_file = move.from % 8;
    rank_t to_rank = move.to / 8;
    file_t to_file = move.to % 8;

    file_t file_dist = abs(from_file - to_file);
    rank_t rank_dist = abs(from_rank - to_rank);

    if (rank_dist == 0 || rank_dist > 2 || file_dist == 0 || file_dist > 2) {
        return false;
    }

    if ((rank_dist == 1 && file_dist != 2) || (file_dist == 1 && rank_dist != 2)) {
        return false;
    }

    do_move(pos, move);
    bb::U64 attackers = attack_bb_to_king(pos, from_piece.color);
    undo_move(pos, move);

    if (attackers != 0) {
        return false;
    }
    return true;
}

static bool is_king_move_legal(Position &pos, Move &move) {
    if (move.to > 63 || move.from > 63 || move.to < 0 || move.from < 0) {
        return false;
    }

    const Piece &from_piece = pos.board[move.from];
    assert(from_piece.type == PieceType::king);
    const Piece &to_piece = pos.board[move.to];

    if (to_piece.type != PieceType::none && to_piece.color == from_piece.color) {
        return false;
    }

    rank_t from_rank = move.from / 8;
    file_t from_file = move.from % 8;
    rank_t to_rank = move.to / 8;
    file_t to_file = move.to % 8;

    file_t file_dist = abs(from_file - to_file);
    rank_t rank_dist = abs(from_rank - to_rank);

    if (rank_dist > 1 || file_dist > 2) {
        return false;
    }

    if (file_dist < 2) {
        do_move(pos, move);
        bb::U64 attackers = attack_bb_to_king(pos, from_piece.color);
        undo_move(pos, move);

        if (attackers != 0) {
            return false;
        }
        return true;
    }
    // handle castle logic

    do_move(pos, move);
    bb::U64 attackers = attack_bb_to_king(pos, from_piece.color);
    undo_move(pos, move);

    if (attackers != 0) {
        return false;
    }
    return true;
}

template <typename IsLegal_t>
inline static void
push_move(Position &pos, sqr_t from, sqr_t to, IsLegal_t is_legal, std::vector<Move> &moves) {
    Move move(from, to);
    bool is_to_in = static_cast<std::size_t>(to) < pos.board.size();
    if (!is_to_in) {
        return;
    }

    move.from = from;
    move.to = to;

    if (is_legal(pos, move)) {
        moves.push_back(move);
    }
}
} // namespace chess
void chess::gen_pawn_moves(Position &pos, sqr_t sqr, std::vector<Move> &moves) {
    assert(sqr >= 0 && sqr < 64);
    const Piece &piece = pos.board[sqr];
    assert(piece.type == PieceType::pawn);
    rank_t rank_sign = 1;
    if (piece.color == Color::black) {
        rank_sign = -1;
    }
    push_move(pos, sqr, sqr + (8 * rank_sign), is_pawn_move_legal, moves);
    push_move(pos, sqr, sqr + (7 * rank_sign), is_pawn_move_legal, moves);
    push_move(pos, sqr, sqr + (9 * rank_sign), is_pawn_move_legal, moves);
    push_move(pos, sqr, sqr + (16 * rank_sign), is_pawn_move_legal, moves);
}
void chess::gen_knight_moves(Position &pos, sqr_t sqr, std::vector<Move> &moves) {
    assert(sqr >= 0 && sqr < 64);
    assert(pos.board[sqr].type == PieceType::knight);
    push_move(pos, sqr, sqr + (2 * 8) + 1, is_knight_move_legal, moves);
    push_move(pos, sqr, sqr + (2 * 8) - 1, is_knight_move_legal, moves);
    push_move(pos, sqr, sqr + (-2 * 8) + 1, is_knight_move_legal, moves);
    push_move(pos, sqr, sqr + (-2 * 8) - 1, is_knight_move_legal, moves);
    push_move(pos, sqr, sqr + (1 * 8) + 2, is_knight_move_legal, moves);
    push_move(pos, sqr, sqr + (-1 * 8) + 2, is_knight_move_legal, moves);
    push_move(pos, sqr, sqr + (1 * 8) - 2, is_knight_move_legal, moves);
    push_move(pos, sqr, sqr + (-1 * 8) - 2, is_knight_move_legal, moves);
}

void chess::gen_bishop_moves(Position &pos, sqr_t sqr, std::vector<Move> &moves) {
    assert(sqr >= 0 && sqr < 64);
    assert(pos.board[sqr].type == PieceType::bishop);

    bb::U64 attack_bb = bb::get_bishop_attack(sqr, pos.bb.get_occupancy()) &
                        ~(pos.bb.get_every_type()[pos.board[sqr].color]);
    while (attack_bb != 0) {
        std::size_t idx = poplsb(attack_bb);
        Move move(sqr, idx);

        do_move(pos, move);
        bb::U64 attackers = attack_bb_to_king(pos, pos.board[sqr].color);
        undo_move(pos, move);

        if (attackers == 0) {
            moves.push_back(move);
        }
    }
}

void chess::gen_rook_moves(Position &pos, sqr_t sqr, std::vector<Move> &moves) {
    assert(sqr >= 0 && sqr < 64);
    assert(pos.board[sqr].type == PieceType::rook);

    bb::U64 attack_bb = bb::get_rook_attack(sqr, pos.bb.get_occupancy()) &
                        ~(pos.bb.get_every_type()[pos.board[sqr].color]);
    while (attack_bb != 0) {
        std::size_t idx = poplsb(attack_bb);
        Move move(sqr, idx);

        do_move(pos, move);
        bb::U64 attackers = attack_bb_to_king(pos, pos.board[sqr].color);
        undo_move(pos, move);

        if (attackers == 0) {
            moves.push_back(move);
        }
    }
}

void chess::gen_queen_moves(Position &pos, sqr_t sqr, std::vector<Move> &moves) {
    assert(sqr >= 0 && sqr < 64);
    assert(pos.board[sqr].type == PieceType::queen);

    bb::U64 occ = pos.bb.get_occupancy();
    bb::U64 attack_bb = bb::get_rook_attack(sqr, occ) | bb::get_bishop_attack(sqr, occ);
    attack_bb &= ~(pos.bb.get_every_type()[pos.board[sqr].color]);
    while (attack_bb != 0) {
        std::size_t idx = poplsb(attack_bb);
        Move move(sqr, idx);

        do_move(pos, move);
        bb::U64 attackers = attack_bb_to_king(pos, pos.board[sqr].color);
        undo_move(pos, move);

        if (attackers == 0) {
            moves.push_back(move);
        }
    }
}

void chess::gen_king_moves(Position &pos, sqr_t sqr, std::vector<Move> &moves) {
    assert(sqr >= 0 && sqr < 64);
    assert(pos.board[sqr].type == PieceType::king);
    push_move(pos, sqr, sqr + 1, is_king_move_legal, moves);
    push_move(pos, sqr, sqr - 1, is_king_move_legal, moves);
    push_move(pos, sqr, sqr + 8, is_king_move_legal, moves);
    push_move(pos, sqr, sqr - 8, is_king_move_legal, moves);
    push_move(pos, sqr, sqr + 7, is_king_move_legal, moves);
    push_move(pos, sqr, sqr - 7, is_king_move_legal, moves);
    push_move(pos, sqr, sqr + 9, is_king_move_legal, moves);
    push_move(pos, sqr, sqr - 9, is_king_move_legal, moves);
}

void chess::gen_legal_moves(Position &pos, std::vector<Move> &moves) {
    for (std::size_t i = 0; i < pos.board.size(); i++) {
        const Piece &piece = pos.board[i];
        if (piece.color != pos.state.active_color) {
            continue;
        }
        switch (piece.type) {
        case PieceType::pawn:   gen_pawn_moves(pos, i, moves); break;
        case PieceType::knight: gen_knight_moves(pos, i, moves); break;
        case PieceType::bishop: gen_bishop_moves(pos, i, moves); break;
        case PieceType::rook:   gen_rook_moves(pos, i, moves); break;
        case PieceType::queen:  gen_queen_moves(pos, i, moves); break;
        case PieceType::king:   gen_king_moves(pos, i, moves); break;
        default:                break;
        }
    }
}
