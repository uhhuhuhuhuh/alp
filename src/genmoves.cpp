#include "genmoves.hpp"
#include "board.hpp"
#include "gamestate.hpp"
#include "pos.hpp"
#include <cassert>

namespace chess {
static bool is_pawn_move_legal(const GameState &state, Move &move) {
    if (move.to > 63 || move.from > 63 || move.to < 0 || move.from < 0) {
        return false;
    }

    const Piece &from_piece = state.board[move.from];
    assert(from_piece.type == PieceType::pawn);
    const Piece &to_piece = state.board[move.to];

    bool is_from_white = from_piece.color == PieceColor::white;
    rank_t rank_sign = 1;
    if (!is_from_white) {
        rank_sign = -1;
    }

    if (to_piece.color == from_piece.color) {
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

    if (to_piece.color == from_piece.color) {
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

    if (to_piece.color == from_piece.color) {
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

// does not check if the piece is jumping over other pieces
bool is_sliding_move_legal(const GameState &state, const Move &move) {
    if (move.from > 63 || move.to > 63 || move.from < 0 || move.to < 0) {
        return false;
    }
    if (state.board[move.from].color == state.board[move.to].color) {
        return false;
    }
    return true;
}

// returns if a sliding move should end
template <typename IsLegal_t>
inline static bool push_move(
    const GameState &state,
    Move &move,
    pos_t from,
    pos_t to,
    IsLegal_t is_legal,
    std::vector<Move> &moves
) {
    bool is_to_in = static_cast<size_t>(to) < state.board.size();
    if (!is_to_in) {
        return true;
    }

    move.from = from;
    move.to = to;
    move.to_piece = state.board[to];

    bool is_capture = move.to_piece.type != PieceType::none;

    if (is_legal(state, move)) {
        moves.push_back(move);
    }

    return is_capture;
}
} // namespace chess
void chess::gen_pawn_moves(const GameState &state, pos_t pos, std::vector<Move> &moves) {
    const Piece &piece = state.board[pos];
    assert(piece.type == PieceType::pawn && piece.color != PieceColor::none);
    Move move;
    rank_t rank_sign = 1;
    if (piece.color == PieceColor::black) {
        rank_sign = -1;
    }
    push_move(state, move, pos, pos + (8 * rank_sign), is_pawn_move_legal, moves);
    push_move(state, move, pos, pos + (7 * rank_sign), is_pawn_move_legal, moves);
    push_move(state, move, pos, pos + (9 * rank_sign), is_pawn_move_legal, moves);
    push_move(state, move, pos, pos + (16 * rank_sign), is_pawn_move_legal, moves);
}
void chess::gen_knight_moves(const GameState &state, pos_t pos, std::vector<Move> &moves) {
    const Piece &piece = state.board[pos];
    assert(piece.type == PieceType::knight && piece.color != PieceColor::none);
    Move move;
    push_move(state, move, pos, pos + (2 * 8) + 1, is_knight_move_legal, moves);
    push_move(state, move, pos, pos + (2 * 8) - 1, is_knight_move_legal, moves);
    push_move(state, move, pos, pos + (-2 * 8) + 1, is_knight_move_legal, moves);
    push_move(state, move, pos, pos + (-2 * 8) - 1, is_knight_move_legal, moves);
    push_move(state, move, pos, pos + (1 * 8) + 2, is_knight_move_legal, moves);
    push_move(state, move, pos, pos + (-1 * 8) + 2, is_knight_move_legal, moves);
    push_move(state, move, pos, pos + (1 * 8) - 2, is_knight_move_legal, moves);
    push_move(state, move, pos, pos + (-1 * 8) - 2, is_knight_move_legal, moves);
}

namespace chess {
inline static void push_sliding(
    const GameState &state,
    pos_t pos,
    rank_t rank_change,
    file_t file_change,
    size_t up_to,
    std::vector<Move> &moves
) {
    const pos_t pos_change = file_change + (rank_change * 8);
    Move move;
    pos_t to = pos;
    for (size_t i = 0; i < up_to; i++) {
        to += pos_change;
        if (push_move(state, move, pos, to, is_sliding_move_legal, moves)) {
            break;
        }
    }
}
} // namespace chess

void chess::gen_bishop_moves(const GameState &state, pos_t pos, std::vector<Move> &moves) {
    rank_t rank = pos / 8;
    file_t file = pos % 8;
    push_sliding(state, pos, 1, 1, std::min(7 - rank, 7 - file), moves);
    push_sliding(state, pos, 1, -1, std::min(7 - rank, file), moves);
    push_sliding(state, pos, -1, 1, std::min(rank, 7 - file), moves);
    push_sliding(state, pos, -1, -1, std::min(rank, file), moves);
}

void chess::gen_rook_moves(const GameState &state, pos_t pos, std::vector<Move> &moves) {
    rank_t rank = pos / 8;
    file_t file = pos % 8;
    push_sliding(state, pos, 1, 0, 7 - rank, moves);
    push_sliding(state, pos, -1, 0, rank, moves);
    push_sliding(state, pos, 0, 1, 7 - file, moves);
    push_sliding(state, pos, 0, -1, file, moves);
}

void chess::gen_queen_moves(const GameState &state, pos_t pos, std::vector<Move> &moves) {
    gen_rook_moves(state, pos, moves);
    gen_bishop_moves(state, pos, moves);
}

void chess::gen_king_moves(const GameState &state, pos_t pos, std::vector<Move> &moves) {
    Move move;
    push_move(state, move, pos, pos + 1, is_king_move_legal, moves);
    push_move(state, move, pos, pos - 1, is_king_move_legal, moves);
    push_move(state, move, pos, pos + 8, is_king_move_legal, moves);
    push_move(state, move, pos, pos - 8, is_king_move_legal, moves);
    push_move(state, move, pos, pos + 7, is_king_move_legal, moves);
    push_move(state, move, pos, pos - 7, is_king_move_legal, moves);
    push_move(state, move, pos, pos + 9, is_king_move_legal, moves);
    push_move(state, move, pos, pos - 9, is_king_move_legal, moves);
}

void chess::gen_legal_moves(const GameState &state, std::vector<Move> &moves) {
    for (size_t i = 0; i < state.board.size(); i++) {
        const Piece &piece = state.board[i];
        if (piece.color != state.active_color) {
            continue;
        }
        switch (piece.type) {
        case PieceType::pawn:
            gen_pawn_moves(state, i, moves);
            break;
        case PieceType::knight:
            gen_knight_moves(state, i, moves);
            break;
        case PieceType::bishop:
            gen_bishop_moves(state, i, moves);
            break;
        case PieceType::rook:
            gen_rook_moves(state, i, moves);
            break;
        case PieceType::queen:
            gen_queen_moves(state, i, moves);
            break;
        case PieceType::king:
            gen_king_moves(state, i, moves);
            break;
        default:
            break;
        }
    }
}
