#include "bitboard.hpp"
#include "constbb.hpp"
#include "square.hpp"

#include <ostream>

namespace bb = chess::bb;

void bb::out_bitboard(U64 bitboard, std::ostream &out) {
    // irank = inverted rank
    for (rank_t irank = 0; irank < 8; irank++) {
        rank_t rank = 7 - irank;
        out << rank + 1 << "  ";
        for (file_t file = 0; file < 8; file++) {
            sqr_t sqr = rank * 8 + file;
            if (bitboard & (1ULL << sqr)) {
                out << "1 ";
                continue;
            }
            out << "0 ";
        }
        out << '\n';
    }
    out << '\n';
    out << "   A B C D E F G H\n";
}

bb::U64 bb::get_bishop_attack(sqr_t sqr, U64 occ) {
    occ &= bishop_tbl[sqr].mask;
    occ *= bishop_tbl[sqr].magic;
    occ >>= (64 - bishop_bit_idxs[sqr]);
    return bishop_attacks[bishop_offsets[sqr] + occ];
}
bb::U64 bb::get_rook_attack(sqr_t sqr, U64 occ) {
    occ &= rook_tbl[sqr].mask;
    occ *= rook_tbl[sqr].magic;
    occ >>= (64 - rook_bit_idxs[sqr]);
    return rook_attacks[rook_offsets[sqr] + occ];
}

void bb::PieceBB::reset_every_type(Color color) {
    every_type[color] = pawns[color] | knights[color] | bishops[color] | rooks[color] |
                        queens[color] | kings[color];
}

void bb::PieceBB::set_pawns(Color color, U64 bitboard) {
    pawns[color] = bitboard;
    reset_every_type(color);
}
void bb::PieceBB::set_knights(Color color, U64 bitboard) {
    knights[color] = bitboard;
    reset_every_type(color);
}
void bb::PieceBB::set_bishops(Color color, U64 bitboard) {
    bishops[color] = bitboard;
    reset_every_type(color);
}
void bb::PieceBB::set_rooks(Color color, U64 bitboard) {
    rooks[color] = bitboard;
    reset_every_type(color);
}
void bb::PieceBB::set_queens(Color color, U64 bitboard) {
    queens[color] = bitboard;
    reset_every_type(color);
}
void bb::PieceBB::set_kings(Color color, U64 bitboard) {
    kings[color] = bitboard;
    reset_every_type(color);
}

template <typename set_bitboard_t>
void bb::PieceBB::set_bitboard_func(PieceType type, Color color, set_bitboard_t set_func) {
    switch (type) {
    case PieceType::pawn:   set_pawns(color, set_func(pawns[color])); break;
    case PieceType::knight: set_knights(color, set_func(knights[color])); break;
    case PieceType::bishop: set_bishops(color, set_func(bishops[color])); break;
    case PieceType::rook:   set_rooks(color, set_func(rooks[color])); break;
    case PieceType::queen:  set_queens(color, set_func(queens[color])); break;
    case PieceType::king:   set_kings(color, set_func(kings[color])); break;
    default:                break;
    }
}

void bb::PieceBB::set_bitboard(PieceType type, Color color, U64 bitboard) {
    set_bitboard_func(type, color, [&](U64) { return bitboard; });
}

void bb::PieceBB::set_or_bitboard(PieceType type, Color color, U64 bitboard) {
    set_bitboard_func(type, color, [&](U64 board) { return board | bitboard; });
}

void bb::PieceBB::set_and_bitboard(PieceType type, Color color, U64 bitboard) {
    set_bitboard_func(type, color, [&](U64 board) { return board & bitboard; });
}
