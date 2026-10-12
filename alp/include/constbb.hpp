#pragma once

#include "piece_color.hpp"
#include "square.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <vector>
namespace chess::bb {
using U64 = uint64_t;

constexpr std::array<U64, 8> files = {
    0x101010101010101ULL,
    0x202020202020202ULL,
    0x404040404040404ULL,
    0x808080808080808ULL,
    0x1010101010101010ULL,
    0x2020202020202020ULL,
    0x4040404040404040ULL,
    0x8080808080808080ULL
};

constexpr std::array<U64, 8> ranks = {
    0x00000000000000FFULL,
    0x000000000000FF00ULL,
    0x0000000000FF0000ULL,
    0x00000000FF000000ULL,
    0x000000FF00000000ULL,
    0x0000FF0000000000ULL,
    0x00FF000000000000ULL,
    0xFF00000000000000ULL
};

constexpr U64 white_sqrs = 0x55AA55AA55AA55AAULL;
constexpr U64 black_sqrs = ~white_sqrs;

constexpr std::array<int, 64> bishop_bit_idxs = {6, 5, 5, 5, 5, 5, 5, 6, 5, 5, 5, 5, 5, 5, 5, 5,
                                                 5, 5, 7, 7, 7, 7, 5, 5, 5, 5, 7, 9, 9, 7, 5, 5,
                                                 5, 5, 7, 9, 9, 7, 5, 5, 5, 5, 7, 7, 7, 7, 5, 5,
                                                 5, 5, 5, 5, 5, 5, 5, 5, 6, 5, 5, 5, 5, 5, 5, 6};

constexpr std::array<int, 64> rook_bit_idxs = {12, 11, 11, 11, 11, 11, 11, 12, 11, 10, 10, 10, 10,
                                               10, 10, 11, 11, 10, 10, 10, 10, 10, 10, 11, 11, 10,
                                               10, 10, 10, 10, 10, 11, 11, 10, 10, 10, 10, 10, 10,
                                               11, 11, 10, 10, 10, 10, 10, 10, 11, 11, 10, 10, 10,
                                               10, 10, 10, 11, 12, 11, 11, 11, 11, 11, 11, 12};

constexpr auto slider_between = [] {
    std::array<std::array<U64, 64>, 64> slider_between{};

    auto abs = [](sqr_t x) { return x < 0 ? -x : x; };

    for (sqr_t from = 0; from < 64; from++) {
        for (sqr_t to = 0; to < 64; to++) {
            rank_t dr = to / 8 - from / 8;
            file_t df = to % 8 - from % 8;

            if (dr == 0 && df == 0) {
                continue;
            }

            sqr_t dir = 0;
            if (df == 0) {
                dir = dr > 0 ? 8 : -8;
            } else if (dr == 0) {
                dir = df > 0 ? 1 : -1;
            } else if (abs(df) == abs(dr)) {
                dir = (dr > 0 ? 8 : -8) + (df > 0 ? 1 : -1);
            } else {
                continue;
            }

            for (sqr_t between = from + dir; between != to; between += dir) {
                slider_between[from][to] |= 1ULL << between;
            }
        }
    }

    return slider_between;
}();

constexpr std::size_t bishop_attacks_size = [] {
    std::size_t size = 0;
    for (size_t i = 0; i < bishop_bit_idxs.size(); i++) {
        size += (1ULL << bishop_bit_idxs[i]);
    }
    return size;
}();
constexpr std::size_t rook_attacks_size = [] {
    std::size_t size = 0;
    for (size_t i = 0; i < rook_bit_idxs.size(); i++) {
        size += (1ULL << rook_bit_idxs[i]);
    }
    return size;
}();

constexpr auto pawn_attacks = [] {
    ColorArray<std::array<bb::U64, 64>> pawn_attacks{};

    for (std::size_t i = 0; i < 64; i++) {
        rank_t rank = i / 8;
        rank_t file = i % 8;
        bool can_left = file > 0;
        bool can_right = file < 7;
        if (rank < 7) {
            if (can_left) {
                pawn_attacks[Color::white][i] |= 1ULL << (((rank + 1) * 8) + (file - 1));
            }
            if (can_right) {
                pawn_attacks[Color::white][i] |= 1ULL << (((rank + 1) * 8) + (file + 1));
            }
        }
        if (rank > 0) {
            if (can_left) {
                pawn_attacks[Color::black][i] |= 1ULL << (((rank - 1) * 8) + (file - 1));
            }
            if (can_right) {
                pawn_attacks[Color::black][i] |= 1ULL << (((rank - 1) * 8) + (file + 1));
            }
        }
    }

    return pawn_attacks;
}();

template <std::size_t size>
constexpr std::array<U64, 64> get_attacks(std::array<std::array<int, 2>, size> offsets) {
    std::array<U64, 64> attacks{};

    for (std::size_t i = 0; i < 64; i++) {
        rank_t rank = i / 8;
        rank_t file = i % 8;

        for (auto &[dr, df] : offsets) {
            rank_t new_rank = rank + dr;
            file_t new_file = file + df;

            if (new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8) {
                attacks[i] |= 1ULL << (new_rank * 8 + new_file);
            }
        }
    }

    return attacks;
}

constexpr auto knight_attacks = get_attacks(
    std::array<std::array<int, 2>, 8>{
        std::array<int, 2>{2,  1 },
        std::array<int, 2>{2,  -1},
        std::array<int, 2>{-2, 1 },
        std::array<int, 2>{-2, -1},
        std::array<int, 2>{1,  2 },
        std::array<int, 2>{1,  -2},
        std::array<int, 2>{-1, 2 },
        std::array<int, 2>{-1, -2}
}
);
constexpr auto king_attacks = get_attacks(
    std::array<std::array<int, 2>, 8>{
        std::array<int, 2>{1,  1 },
        std::array<int, 2>{1,  -1},
        std::array<int, 2>{1,  0 },
        std::array<int, 2>{0,  1 },
        std::array<int, 2>{0,  -1},
        std::array<int, 2>{-1, 0 },
        std::array<int, 2>{-1, 1 },
        std::array<int, 2>{-1, -1}
}
);

inline U64 bishop_rays(bb::U64 occ, sqr_t sqr) {
    U64 bb = 0ULL;

    rank_t t_rank = sqr / 8;
    file_t t_file = sqr % 8;
    rank_t rank = 0;
    file_t file = 0;

    for (rank = t_rank + 1, file = t_file + 1; rank < 8 && file < 8; rank++, file++) {
        U64 at_bb = 1ULL << ((rank * 8) + file);
        bb |= at_bb;
        if ((occ & at_bb) != 0)
            break;
    }
    for (rank = t_rank + 1, file = t_file - 1; rank < 8 && file >= 0; rank++, file--) {
        U64 at_bb = 1ULL << ((rank * 8) + file);
        bb |= at_bb;
        if ((occ & at_bb) != 0)
            break;
    }
    for (rank = t_rank - 1, file = t_file + 1; rank >= 0 && file < 8; rank--, file++) {
        U64 at_bb = 1ULL << ((rank * 8) + file);
        bb |= at_bb;
        if ((occ & at_bb) != 0)
            break;
    }
    for (rank = t_rank - 1, file = t_file - 1; rank >= 0 && file >= 0; rank--, file--) {
        U64 at_bb = 1ULL << ((rank * 8) + file);
        bb |= at_bb;
        if ((occ & at_bb) != 0)
            break;
    }

    return bb;
}

inline U64 rook_rays(bb::U64 occ, sqr_t sqr) {
    U64 bb = 0ULL;

    rank_t t_rank = sqr / 8;
    file_t t_file = sqr % 8;
    rank_t rank = 0;
    file_t file = 0;

    for (rank = t_rank + 1; rank < 8; rank++) {
        U64 at_bb = 1ULL << ((rank * 8) + t_file);
        bb |= at_bb;
        if ((occ & at_bb) != 0)
            break;
    }
    for (rank = t_rank - 1; rank >= 0; rank--) {
        U64 at_bb = 1ULL << ((rank * 8) + t_file);
        bb |= at_bb;
        if ((occ & at_bb) != 0)
            break;
    }
    for (file = t_file + 1; file < 8; file++) {
        U64 at_bb = 1ULL << ((t_rank * 8) + file);
        bb |= at_bb;
        if ((occ & at_bb) != 0)
            break;
    }
    for (file = t_file - 1; file >= 0; file--) {
        U64 at_bb = 1ULL << ((t_rank * 8) + file);
        bb |= at_bb;
        if ((occ & at_bb) != 0)
            break;
    }

    return bb;
}

inline U64 xorshift64star(uint64_t &x) {
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    return x * 0x2545F4914F6CDD1DULL;
}

struct SeenAttackBB {
    U64 attack = 0ULL;
    uint32_t gen = 0;
    SeenAttackBB() = default;
};

template <std::size_t size> struct OffsetAttacks {
    std::array<U64, size> data{};
    std::array<size_t, 64> offsets{};
    OffsetAttacks() = default;
};

template <typename ray_t, std::size_t size>
inline U64 find_magic(
    U64 mask, bb::U64 bit_idx, sqr_t sqr, bb::U64 &seed, OffsetAttacks<size> &attacks, ray_t ray,
    size_t &offset
) {
    const std::size_t table_size = 1ULL << bit_idx;
    std::vector<U64> occupancies;
    std::vector<U64> ray_attacks;
    occupancies.reserve(table_size);
    ray_attacks.reserve(table_size);

    for (U64 subset = mask;; subset = (subset - 1) & mask) {
        occupancies.push_back(subset);
        ray_attacks.push_back(ray(subset, sqr));
        if (subset == 0) {
            break;
        }
    }

    std::vector<SeenAttackBB> attack_hist(table_size);
    for (uint32_t gen = 0;; gen++) {
        bool valid = true;
        U64 magic = xorshift64star(seed) & xorshift64star(seed) & xorshift64star(seed);

        for (std::size_t i = 0; i < occupancies.size(); i++) {
            U64 occ = occupancies[i];
            std::size_t idx = (occ * magic) >> (64 - bit_idx);

            if (attack_hist[idx].gen != gen) {
                attack_hist[idx].attack = ray_attacks[i];
                attack_hist[idx].gen = gen;
                continue;
            }

            if (attack_hist[idx].attack != ray_attacks[i]) {
                valid = false;
                break;
            }
        }

        if (!valid) {
            continue;
        }
        for (std::size_t i = 0; i < occupancies.size(); i++) {
            U64 occ = occupancies[i];
            std::size_t idx = (occ * magic) >> (64 - bit_idx);
            attacks.data[offset + idx] = ray_attacks[i];
        }
        attacks.offsets[sqr] = offset;
        offset += (1ULL << bit_idx);
        return magic;
    }
}

struct GMagic {
    U64 mask;
    U64 magic;
};
template <std::size_t size> struct AttacksTbl {
    std::array<GMagic, 64> gmagic;
    OffsetAttacks<size> atks;
};

inline AttacksTbl<bishop_attacks_size> get_bishop_attack_tbl() {
    AttacksTbl<bishop_attacks_size> attack_tbl;
    std::size_t offset = 0;
    // a random starting seed
    uint64_t seed = 578;

    for (std::size_t i = 0; i < 64; i++) {
        U64 mask = 0ULL;

        rank_t r;
        file_t f;
        // t stands for target
        rank_t tr = i / 8;
        file_t tf = i % 8;
        for (r = tr + 1, f = tf + 1; r < 7 && f < 7; r++, f++)
            mask |= (1ULL << (r * 8 + f));
        for (r = tr + 1, f = tf - 1; r < 7 && f > 0; r++, f--)
            mask |= (1ULL << (r * 8 + f));
        for (r = tr - 1, f = tf + 1; r > 0 && f < 7; r--, f++)
            mask |= (1ULL << (r * 8 + f));
        for (r = tr - 1, f = tf - 1; r > 0 && f > 0; r--, f--)
            mask |= (1ULL << (r * 8 + f));

        attack_tbl.gmagic[i].mask = mask;
        attack_tbl.gmagic[i].magic =
            find_magic(mask, bishop_bit_idxs[i], i, seed, attack_tbl.atks, bishop_rays, offset);
    }
    return attack_tbl;
}
inline const auto bishop_atk_tbl = get_bishop_attack_tbl();

inline AttacksTbl<rook_attacks_size> get_rook_attack_tbl() {
    AttacksTbl<rook_attacks_size> attack_tbl;
    std::size_t offset = 0;
    // a random starting seed
    uint64_t seed = 109;

    for (std::size_t i = 0; i < 64; i++) {
        U64 mask = 0LL;
        // t stands for target
        rank_t t_rank = i / 8;
        rank_t t_file = i % 8;
        for (rank_t rank = t_rank + 1; rank < 7; rank++)
            mask |= (1ULL << (rank * 8 + t_file));
        for (rank_t rank = t_rank - 1; rank > 0; rank--)
            mask |= (1ULL << (rank * 8 + t_file));
        for (file_t file = t_file + 1; file < 7; file++)
            mask |= (1ULL << (t_rank * 8 + file));
        for (file_t file = t_file - 1; file > 0; file--)
            mask |= (1ULL << (t_rank * 8 + file));

        attack_tbl.gmagic[i].mask = mask;
        attack_tbl.gmagic[i].magic =
            find_magic(mask, rook_bit_idxs[i], i, seed, attack_tbl.atks, rook_rays, offset);
    }

    return attack_tbl;
}
inline const auto rook_atk_tbl = get_rook_attack_tbl();
} // namespace chess::bb
