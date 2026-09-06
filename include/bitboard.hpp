#pragma once

#include "constbb.hpp"
#include "piece.hpp"
#include "square.hpp"

#include <cstdint>
#include <ostream>

namespace chess::bb {
using U64 = uint64_t;
using BBColorArray = ColorArray<U64>;

inline constexpr bool is_sqr_white(sqr_t sqr) { return (sqr & white_sqrs); }

inline void output_bb(U64 bitboard, std::ostream &out) {
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

inline U64 get_bishop_attack(sqr_t sqr, U64 occ) {
    occ &= bishop_tbl[sqr].mask;
    occ *= bishop_tbl[sqr].magic;
    occ >>= (64 - bishop_bit_idxs[sqr]);
    return bishop_attacks[bishop_offsets[sqr] + occ];
}
inline U64 get_rook_attack(sqr_t sqr, U64 occ) {
    occ &= rook_tbl[sqr].mask;
    occ *= rook_tbl[sqr].magic;
    occ >>= (64 - rook_bit_idxs[sqr]);
    return rook_attacks[rook_offsets[sqr] + occ];
}

inline U64 xray_bishop_attack(U64 occ, U64 blockers, sqr_t bishop_sqr) {
    U64 attacks = get_bishop_attack(bishop_sqr, occ);
    blockers &= attacks;
    return attacks ^ get_bishop_attack(bishop_sqr, occ ^ blockers);
}
inline U64 xray_rook_attack(U64 occ, U64 blockers, sqr_t rook_sqr) {
    U64 attacks = get_rook_attack(rook_sqr, occ);
    blockers &= attacks;
    return attacks ^ get_rook_attack(rook_sqr, occ ^ blockers);
}

class PieceBB {
  private:
    static constexpr std::size_t pieces_size = static_cast<uint8_t>(PieceType::king);
    std::array<BBColorArray, pieces_size> pieces{};
    BBColorArray m_every_type{};
    bb::U64 m_occupancy = 0ULL;

    inline void reset_calculated(Color color) {
        m_every_type[color] = 0;
        for (std::size_t i = 0; i < pieces_size; i++) {
            m_every_type[color] |= pieces[i][color];
        }
        m_occupancy = m_every_type[Color::white] | m_every_type[Color::black];
    }

  public:
    inline const BBColorArray &get(PieceType type) const {
        return pieces[static_cast<std::size_t>(type) - 1];
    }
    inline U64 get_all(PieceType type) const {
        std::size_t idx = static_cast<std::size_t>(type) - 1;
        return pieces[idx][Color::white] | pieces[idx][Color::black];
    }
    inline const BBColorArray &every_type() const { return m_every_type; };
    inline U64 occupancy() const { return m_occupancy; };

    inline void place(PieceType type, Color color, sqr_t sqr) {
        U64 sqr_bb = 1ULL << sqr;
        pieces[static_cast<std::size_t>(type) - 1][color] |= sqr_bb;
        m_every_type[color] |= sqr_bb;
        m_occupancy |= sqr_bb;
    }
    inline void remove(PieceType type, Color color, sqr_t sqr) {
        U64 mask = ~(1ULL << sqr);
        pieces[static_cast<std::size_t>(type) - 1][color] &= mask;
        m_every_type[color] &= mask;
        m_occupancy &= mask;
    }

    inline void move(PieceType type, Color color, sqr_t from, sqr_t to) {
        U64 to_from_bb = (1ULL << from) | (1ULL << to);
        pieces[static_cast<std::size_t>(type) - 1][color] ^= to_from_bb;
        m_every_type[color] ^= to_from_bb;
        m_occupancy ^= to_from_bb;
    }
    inline void capture(
        PieceType type, Color color, sqr_t from, PieceType captured_type, Color captured_color,
        sqr_t to
    ) {
        U64 from_bb = 1ULL << from;
        U64 to_bb = 1ULL << to;
        U64 to_from_bb = from_bb | to_bb;

        pieces[static_cast<std::size_t>(type) - 1][color] ^= to_from_bb;
        m_every_type[color] ^= to_from_bb;
        pieces[static_cast<std::size_t>(captured_type) - 1][captured_color] &= ~to_bb;
        m_every_type[captured_color] &= ~to_bb;

        m_occupancy ^= from_bb;
    }
};
} // namespace chess::bb
