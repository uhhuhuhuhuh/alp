#include "bitboard.hpp"
#include "constbb.hpp"
#include "square.hpp"

#include <ostream>

namespace bb = chess::bb;

void bb::output_bb(U64 bitboard, std::ostream &out) {
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

void bb::PieceBB::set_bitboard(PieceType type, Color color, U64 bitboard) {
    switch (type) {
    case PieceType::pawn:   set_pawns(color, bitboard); break;
    case PieceType::knight: set_knights(color, bitboard); break;
    case PieceType::bishop: set_bishops(color, bitboard); break;
    case PieceType::rook:   set_rooks(color, bitboard); break;
    case PieceType::queen:  set_queens(color, bitboard); break;
    case PieceType::king:   set_kings(color, bitboard); break;
    default:                break;
    }
}

void bb::PieceBB::set_or_bitboard(PieceType type, Color color, U64 bitboard) {
    switch (type) {
    case PieceType::pawn:   set_pawns(color, pawns[color] | bitboard); break;
    case PieceType::knight: set_knights(color, knights[color] | bitboard); break;
    case PieceType::bishop: set_bishops(color, bishops[color] | bitboard); break;
    case PieceType::rook:   set_rooks(color, rooks[color] | bitboard); break;
    case PieceType::queen:  set_queens(color, queens[color] | bitboard); break;
    case PieceType::king:   set_kings(color, kings[color] | bitboard); break;
    default:                break;
    }
}

void bb::PieceBB::set_and_bitboard(PieceType type, Color color, U64 bitboard) {
    switch (type) {
    case PieceType::pawn:   set_pawns(color, pawns[color] & bitboard); break;
    case PieceType::knight: set_knights(color, knights[color] & bitboard); break;
    case PieceType::bishop: set_bishops(color, bishops[color] & bitboard); break;
    case PieceType::rook:   set_rooks(color, rooks[color] & bitboard); break;
    case PieceType::queen:  set_queens(color, queens[color] & bitboard); break;
    case PieceType::king:   set_kings(color, kings[color] & bitboard); break;
    default:                break;
    }
}