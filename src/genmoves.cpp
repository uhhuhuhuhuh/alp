#include "genmoves.hpp"
#include "bitboard.hpp"
#include "gamestate.hpp"
#include "piece.hpp"
#include "pos.hpp"
#include <cassert>

// returns index of least significant bit.
static std::size_t poplsb(chess::bb::U64 &x) {
    auto index = std::countr_zero(x);
    x &= x - 1;
    return index;
}

namespace chess {
static bool is_pawn_move_legal(const GameState &state, Move &move) {
    if (move.to > 63 || move.from > 63 || move.to < 0 || move.from < 0) {
        return false;
    }

    const Piece &from_piece = state.board[move.from];
    assert(from_piece.type == PieceType::pawn);
    const Piece &to_piece = state.board[move.to];

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
            if (state.board[move.to - 8 * rank_sign].type != PieceType::none) {
                return false;
            }
        }
    }

    if ((to_rank == 7 && is_from_white) || (to_rank == 0 && !is_from_white)) {
        // TODO: add promotion
    }

    return true;
}

static bool is_knight_move_legal(const GameState &state, const Move &move) {
    if (move.to > 63 || move.from > 63 || move.to < 0 || move.from < 0) {
        return false;
    }

    const Piece &from_piece = state.board[move.from];
    assert(from_piece.type == PieceType::knight);
    const Piece &to_piece = state.board[move.to];

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
    return true;
}

static bool is_king_move_legal(const GameState &state, const Move &move) {
    if (move.to > 63 || move.from > 63 || move.to < 0 || move.from < 0) {
        return false;
    }

    const Piece &from_piece = state.board[move.from];
    assert(from_piece.type == PieceType::king);
    const Piece &to_piece = state.board[move.to];

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
        return true;
    }
    // handle castle logic
    return true;
}

template <typename IsLegal_t>
inline static void push_move(
    const GameState &state, pos_t from, pos_t to, IsLegal_t is_legal, std::vector<Move> &moves
) {
    Move move(from, to);
    bool is_to_in = static_cast<std::size_t>(to) < state.board.size();
    if (!is_to_in) {
        return;
    }

    move.from = from;
    move.to = to;
    move.to_piece = state.board[to];

    if (is_legal(state, move)) {
        moves.push_back(move);
    }
}
} // namespace chess
void chess::gen_pawn_moves(const GameState &state, pos_t pos, std::vector<Move> &moves) {
    assert(pos >= 0 && pos < 64);
    const Piece &piece = state.board[pos];
    assert(piece.type == PieceType::pawn);
    rank_t rank_sign = 1;
    if (piece.color == Color::black) {
        rank_sign = -1;
    }
    push_move(state, pos, pos + (8 * rank_sign), is_pawn_move_legal, moves);
    push_move(state, pos, pos + (7 * rank_sign), is_pawn_move_legal, moves);
    push_move(state, pos, pos + (9 * rank_sign), is_pawn_move_legal, moves);
    push_move(state, pos, pos + (16 * rank_sign), is_pawn_move_legal, moves);
}
void chess::gen_knight_moves(const GameState &state, pos_t pos, std::vector<Move> &moves) {
    assert(pos >= 0 && pos < 64);
    assert(state.board[pos].type == PieceType::knight);
    push_move(state, pos, pos + (2 * 8) + 1, is_knight_move_legal, moves);
    push_move(state, pos, pos + (2 * 8) - 1, is_knight_move_legal, moves);
    push_move(state, pos, pos + (-2 * 8) + 1, is_knight_move_legal, moves);
    push_move(state, pos, pos + (-2 * 8) - 1, is_knight_move_legal, moves);
    push_move(state, pos, pos + (1 * 8) + 2, is_knight_move_legal, moves);
    push_move(state, pos, pos + (-1 * 8) + 2, is_knight_move_legal, moves);
    push_move(state, pos, pos + (1 * 8) - 2, is_knight_move_legal, moves);
    push_move(state, pos, pos + (-1 * 8) - 2, is_knight_move_legal, moves);
}

void chess::gen_bishop_moves(const GameState &state, pos_t pos, std::vector<Move> &moves) {
    assert(pos >= 0 && pos < 64);
    assert(state.board[pos].type == PieceType::bishop);

    bb::U64 attack_bb = bb::get_bishop_attack(pos, state.bb.get_occupancy()) &
                        ~(state.bb.get_every_type()[state.board[pos].color]);
    while (attack_bb != 0) {
        std::size_t idx = poplsb(attack_bb);
        Move move(pos, idx);
        // TODO: filter out self checks.
        moves.push_back(move);
    }
}

void chess::gen_rook_moves(const GameState &state, pos_t pos, std::vector<Move> &moves) {
    assert(pos >= 0 && pos < 64);
    assert(state.board[pos].type == PieceType::rook);

    bb::U64 attack_bb = bb::get_rook_attack(pos, state.bb.get_occupancy()) &
                        ~(state.bb.get_every_type()[state.board[pos].color]);
    while (attack_bb != 0) {
        std::size_t idx = poplsb(attack_bb);
        Move move(pos, idx);
        // TODO: filter out self checks.
        moves.push_back(move);
    }
}

void chess::gen_queen_moves(const GameState &state, pos_t pos, std::vector<Move> &moves) {
    assert(pos >= 0 && pos < 64);
    assert(state.board[pos].type == PieceType::queen);

    bb::U64 occ = state.bb.get_occupancy();
    bb::U64 attack_bb = bb::get_rook_attack(pos, occ) | bb::get_bishop_attack(pos, occ);
    attack_bb &= ~(state.bb.get_every_type()[state.board[pos].color]);
    while (attack_bb != 0) {
        std::size_t idx = poplsb(attack_bb);
        Move move(pos, idx);
        // TODO: filter out self checks.
        moves.push_back(move);
    }
}

void chess::gen_king_moves(const GameState &state, pos_t pos, std::vector<Move> &moves) {
    assert(pos >= 0 && pos < 64);
    assert(state.board[pos].type == PieceType::king);
    push_move(state, pos, pos + 1, is_king_move_legal, moves);
    push_move(state, pos, pos - 1, is_king_move_legal, moves);
    push_move(state, pos, pos + 8, is_king_move_legal, moves);
    push_move(state, pos, pos - 8, is_king_move_legal, moves);
    push_move(state, pos, pos + 7, is_king_move_legal, moves);
    push_move(state, pos, pos - 7, is_king_move_legal, moves);
    push_move(state, pos, pos + 9, is_king_move_legal, moves);
    push_move(state, pos, pos - 9, is_king_move_legal, moves);
}

void chess::gen_legal_moves(const GameState &state, std::vector<Move> &moves) {
    for (std::size_t i = 0; i < state.board.size(); i++) {
        const Piece &piece = state.board[i];
        if (piece.color != state.active_color) {
            continue;
        }
        switch (piece.type) {
        case PieceType::pawn:   gen_pawn_moves(state, i, moves); break;
        case PieceType::knight: gen_knight_moves(state, i, moves); break;
        case PieceType::bishop: gen_bishop_moves(state, i, moves); break;
        case PieceType::rook:   gen_rook_moves(state, i, moves); break;
        case PieceType::queen:  gen_queen_moves(state, i, moves); break;
        case PieceType::king:   gen_king_moves(state, i, moves); break;
        default:                break;
        }
    }
}
