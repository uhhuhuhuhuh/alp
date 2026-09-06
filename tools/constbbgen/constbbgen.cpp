#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

#include "bitboard.hpp"
#include "constbb.hpp"
#include "square.hpp"

namespace bb = chess::bb;
using namespace chess;

std::array<std::array<bb::U64, 64>, 64> get_slider_between() {
    decltype(get_slider_between()) slider_between{};

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
            } else if (std::abs(df) == std::abs(dr)) {
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
}

constexpr std::size_t get_bishop_attacks_size() {
    std::size_t size = 0;
    for (size_t i = 0; i < bb::bishop_bit_idxs.size(); i++) {
        size += (1ULL << bb::bishop_bit_idxs[i]);
    }
    return size;
}
constexpr std::size_t bishop_attacks_size = get_bishop_attacks_size();

constexpr std::size_t get_rook_attacks_size() {
    std::size_t size = 0;
    for (size_t i = 0; i < bb::rook_bit_idxs.size(); i++) {
        size += (1ULL << bb::rook_bit_idxs[i]);
    }
    return size;
}
constexpr std::size_t rook_attacks_size = get_rook_attacks_size();

template <std::size_t size> struct Attacks {
    std::array<bb::U64, size> data{};
    std::array<size_t, 64> offsets{};
    Attacks() = default;
};

bb::ColorArray<std::array<bb::U64, 64>> get_pawn_attacks() {
    bb::ColorArray<std::array<bb::U64, 64>> pawn_attacks{};

    for (std::size_t i = 0; i < 64; i++) {
        rank_t rank = i / 8;
        rank_t file = i % 8;
        bool can_left = file > 0;
        bool can_right = file < 7;
        if (rank < 7) {
            if (can_left)
                pawn_attacks[Color::white][i] |= 1ULL << (((rank + 1) * 8) + (file - 1));
            if (can_right)
                pawn_attacks[Color::white][i] |= 1ULL << (((rank + 1) * 8) + (file + 1));
        }
        if (rank > 0) {
            if (can_left)
                pawn_attacks[Color::black][i] |= 1ULL << (((rank - 1) * 8) + (file - 1));
            if (can_right)
                pawn_attacks[Color::black][i] |= 1ULL << (((rank - 1) * 8) + (file + 1));
        }
    }
    return pawn_attacks;
}

std::array<bb::U64, 64> get_knight_attacks() {
    std::array<bb::U64, 64> knight_attacks{};

    for (std::size_t i = 0; i < 64; i++) {
        rank_t rank = i / 8;
        rank_t file = i % 8;
        constexpr int offsets[][2] = {
            {2,  1 },
            {2,  -1},
            {-2, 1 },
            {-2, -1},
            {1,  2 },
            {1,  -2},
            {-1, 2 },
            {-1, -2}
        };

        for (auto &[dr, df] : offsets) {
            rank_t new_rank = rank + dr;
            file_t new_file = file + df;

            if (new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8) {
                knight_attacks[i] |= 1ULL << (new_rank * 8 + new_file);
            }
        }
    }

    return knight_attacks;
}

std::array<bb::U64, 64> get_king_attacks() {
    std::array<bb::U64, 64> king_attacks{};

    for (std::size_t i = 0; i < 64; i++) {
        rank_t rank = i / 8;
        rank_t file = i % 8;
        constexpr int offsets[][2] = {
            {1,  1 },
            {1,  -1},
            {1,  0 },
            {0,  1 },
            {0,  -1},
            {-1, 0 },
            {-1, 1 },
            {-1, -1}
        };

        for (auto &[dr, df] : offsets) {
            rank_t new_rank = rank + dr;
            file_t new_file = file + df;

            if (new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8) {
                king_attacks[i] |= 1ULL << (new_rank * 8 + new_file);
            }
        }
    }
    return king_attacks;
}

bb::U64 bishop_rays(bb::U64 occ, sqr_t sqr) {
    bb::U64 bb = 0ULL;

    rank_t t_rank = sqr / 8;
    file_t t_file = sqr % 8;
    rank_t rank = 0;
    file_t file = 0;

    for (rank = t_rank + 1, file = t_file + 1; rank < 8 && file < 8; rank++, file++) {
        bb::U64 at_bb = 1ULL << ((rank * 8) + file);
        bb |= at_bb;
        if ((occ & at_bb) != 0) {
            break;
        }
    }
    for (rank = t_rank + 1, file = t_file - 1; rank < 8 && file >= 0; rank++, file--) {
        bb::U64 at_bb = 1ULL << ((rank * 8) + file);
        bb |= at_bb;
        if ((occ & at_bb) != 0) {
            break;
        }
    }
    for (rank = t_rank - 1, file = t_file + 1; rank >= 0 && file < 8; rank--, file++) {
        bb::U64 at_bb = 1ULL << ((rank * 8) + file);
        bb |= at_bb;
        if ((occ & at_bb) != 0) {
            break;
        }
    }
    for (rank = t_rank - 1, file = t_file - 1; rank >= 0 && file >= 0; rank--, file--) {
        bb::U64 at_bb = 1ULL << ((rank * 8) + file);
        bb |= at_bb;
        if ((occ & at_bb) != 0) {
            break;
        }
    }

    return bb;
}

bb::U64 rook_rays(bb::U64 occ, sqr_t sqr) {
    bb::U64 bb = 0ULL;

    rank_t t_rank = sqr / 8;
    file_t t_file = sqr % 8;
    rank_t rank = 0;
    file_t file = 0;

    for (rank = t_rank + 1; rank < 8; rank++) {
        bb::U64 at_bb = 1ULL << ((rank * 8) + t_file);
        bb |= at_bb;
        if ((occ & at_bb) != 0) {
            break;
        }
    }
    for (rank = t_rank - 1; rank >= 0; rank--) {
        bb::U64 at_bb = 1ULL << ((rank * 8) + t_file);
        bb |= at_bb;
        if ((occ & at_bb) != 0) {
            break;
        }
    }
    for (file = t_file + 1; file < 8; file++) {
        bb::U64 at_bb = 1ULL << ((t_rank * 8) + file);
        bb |= at_bb;
        if ((occ & at_bb) != 0) {
            break;
        }
    }
    for (file = t_file - 1; file >= 0; file--) {
        bb::U64 at_bb = 1ULL << ((t_rank * 8) + file);
        bb |= at_bb;
        if ((occ & at_bb) != 0) {
            break;
        }
    }

    return bb;
}

inline bb::U64 xorshift64star(bb::U64 &x) {
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    return x * 0x2545F4914F6CDD1DULL;
}

struct SeenAttackBB {
    bb::U64 attack = 0ULL;
    uint32_t gen = 0;
    SeenAttackBB() = default;
};

template <typename ray_t, std::size_t size>
bb::U64 find_magic(
    bb::U64 mask, bb::U64 bit_idx, sqr_t sqr, bb::U64 &seed, Attacks<size> &attacks, ray_t ray,
    size_t &offset
) {
    const std::size_t table_size = 1ULL << bit_idx;
    std::vector<bb::U64> occupancies;
    std::vector<bb::U64> ray_attacks;
    occupancies.reserve(table_size);
    ray_attacks.reserve(table_size);

    for (bb::U64 subset = mask;; subset = (subset - 1) & mask) {
        occupancies.push_back(subset);
        ray_attacks.push_back(ray(subset, sqr));
        if (subset == 0) {
            break;
        }
    }

    std::vector<SeenAttackBB> attack_hist(table_size);
    uint32_t gen = 0;
    while (true) {
        gen++;
        bool valid = true;
        bb::U64 magic = xorshift64star(seed) & xorshift64star(seed) & xorshift64star(seed);

        for (std::size_t i = 0; i < occupancies.size(); i++) {
            bb::U64 occ = occupancies[i];
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
            bb::U64 occ = occupancies[i];
            std::size_t idx = (occ * magic) >> (64 - bit_idx);
            attacks.data[offset + idx] = ray_attacks[i];
        }
        attacks.offsets[sqr] = offset;
        offset += (1ULL << bit_idx);
        return magic;
    }
}

std::array<bb::GMagic, 64> get_bishop_gmagic(Attacks<bishop_attacks_size> &attacks) {
    std::array<bb::GMagic, 64> bishop_tbl{};
    std::size_t offset = 0;
    // a random starting seed
    bb::U64 seed = 578;

    for (std::size_t i = 0; i < 64; i++) {
        bb::U64 mask = 0ULL;

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

        bishop_tbl[i].mask = mask;
        bishop_tbl[i].magic =
            find_magic(mask, bb::bishop_bit_idxs[i], i, seed, attacks, bishop_rays, offset);
    }
    return bishop_tbl;
}

std::array<bb::GMagic, 64> get_rook_gmagic(Attacks<rook_attacks_size> &attacks) {
    std::array<bb::GMagic, 64> rook_tbl{};
    std::size_t offset = 0;
    // a random starting seed
    bb::U64 seed = 43216;

    for (std::size_t i = 0; i < 64; i++) {
        bb::U64 mask = 0LL;
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

        rook_tbl[i].mask = mask;
        rook_tbl[i].magic =
            find_magic(mask, bb::rook_bit_idxs[i], i, seed, attacks, rook_rays, offset);
    }
    return rook_tbl;
}

void out_color_arr(const bb::ColorArray<std::array<bb::U64, 64>> &arr, std::string_view sv_name) {
    std::cout << "constexpr ColorArray<std::array<U64, 64>> " << sv_name << "({\n";
    for (std::size_t i = 0; i < 2; i++) {
        std::cout << "    std::array<U64, 64>{\n";
        for (std::size_t j = 0; j < arr.data[i].size(); j++) {
            std::cout << "       " << arr.data[i][j] << "ULL";
            if (j < arr.data[i].size() - 1) {
                std::cout << ",";
            }
            std::cout << '\n';
        }
        std::cout << "    }";
        if (i < 1) {
            std::cout << ',';
        }
        std::cout << '\n';
    }
    std::cout << "});\n";
}

template <std::size_t size1, std::size_t size2>
void out_U64_2d_arr(
    const std::array<std::array<bb::U64, size1>, size2> &arr, std::string_view sv_name
) {
    std::cout << "constexpr std::array<std::array<U64, " << size1 << ">, " << size2 << "> "
              << sv_name << " = {\n";
    for (std::size_t i = 0; i < arr.size(); i++) {
        std::cout << "    std::array<U64, " << arr[i].size() << ">{\n";
        for (std::size_t j = 0; j < arr[i].size(); j++) {
            std::cout << "        " << arr[i][j] << "ULL";
            if (j < arr[i].size() - 1) {
                std::cout << ",";
            }
            std::cout << "\n";
        }
        std::cout << "    }" << (i < arr.size() - 1 ? "," : "") << '\n';
    }
    std::cout << "};\n";
}

template <std::size_t size>
void out_U64_arr(const std::array<bb::U64, size> &arr, std::string_view sv_name) {
    std::cout << "constexpr std::array<U64, " << arr.size() << "> " << sv_name << " = {\n";
    for (std::size_t i = 0; i < arr.size(); i++) {
        std::cout << "    " << arr[i] << "ULL";
        if (i < arr.size() - 1) {
            std::cout << ",";
        }
        std::cout << "\n";
    }
    std::cout << "};\n";
}

void out_gmagic_arr(const std::array<bb::GMagic, 64> &tbl, std::string_view sv_name) {
    std::cout << "constexpr std::array<GMagic, 64> " << sv_name << " = {\n";
    for (std::size_t i = 0; i < 64; i++) {
        std::cout << "    bb::GMagic{" << tbl[i].mask << "ULL, " << tbl[i].magic << "ULL}";
        if (i < 63) {
            std::cout << ",";
        }
        std::cout << "\n";
    }
    std::cout << "};\n";
}

int main() {
    auto slider_between = get_slider_between();
    out_U64_2d_arr(slider_between, "slider_between");
    /*
    auto pawn_attacks = get_pawn_attacks();
    auto knight_attacks = get_knight_attacks();
    Attacks<bishop_attacks_size> bishop_attacks;
    auto bishop_tbl = get_bishop_gmagic(bishop_attacks);
    Attacks<rook_attacks_size> rook_attacks;
    auto rook_tbl = get_rook_gmagic(rook_attacks);
    auto king_attacks = get_king_attacks();

    out_color_arr(pawn_attacks, "pawn_attacks");
    out_U64_arr(knight_attacks, "knight_attacks");
    out_gmagic_arr(bishop_tbl, "bishop_tbl");
    out_gmagic_arr(rook_tbl, "rook_tbl");
    out_U64_arr(bishop_attacks.offsets, "bishop_offsets");
    out_U64_arr(rook_attacks.offsets, "rook_offsets");
    out_U64_arr(bishop_attacks.data, "bishop_attacks");
    out_U64_arr(rook_attacks.data, "rook_attacks");
    out_U64_arr(king_attacks, "king_attacks"); */
}
