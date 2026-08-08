#include <array>
#include <cassert>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "bitboard.hpp"
#include "constbb.hpp"
#include "pos.hpp"

namespace bb = chess::bb;
using namespace chess;

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

bb::U64 bishop_rays(bb::U64 occ, pos_t pos) {
    bb::U64 bb = 0ULL;

    rank_t t_rank = pos / 8;
    file_t t_file = pos % 8;
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

bb::U64 rook_rays(bb::U64 occ, pos_t pos) {
    bb::U64 bb = 0ULL;

    rank_t t_rank = pos / 8;
    file_t t_file = pos % 8;
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

bb::U64 xorshift64star(bb::U64 &x) {
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    return x * 0x2545F4914F6CDD1DULL;
}

bool is_magic_valid(
    bb::U64 occ, pos_t pos, std::unordered_map<std::size_t, bb::U64> &attacks, bb::U64 attack_bb
) {
    return attacks.contains(occ) && attacks[occ] != attack_bb;
}

template <typename ray_t, std::size_t size>
bb::U64 find_magic(
    bb::U64 mask, bb::U64 bit_idx, pos_t pos, bb::U64 &seed,
    std::array<std::array<bb::U64, size>, 64> &attacks, ray_t ray
) {
    assert(bit_idx == bb::BISHOP_IDX_BITS || bit_idx == bb::ROOK_IDX_BITS);

    const std::size_t table_size = 1ULL << bit_idx;
    std::vector<bb::U64> occupancies;
    std::vector<bb::U64> ray_attacks;
    occupancies.reserve(table_size);
    ray_attacks.reserve(table_size);

    for (bb::U64 subset = mask;; subset = (subset - 1) & mask) {
        occupancies.push_back(subset);
        ray_attacks.push_back(ray(subset, pos));
        if (subset == 0) {
            break;
        }
    }

    while (true) {
        std::unordered_map<std::size_t, bb::U64> seen;
        bool valid = true;
        bb::U64 magic = xorshift64star(seed) & xorshift64star(seed) & xorshift64star(seed);

        for (std::size_t i = 0; i < occupancies.size(); i++) {
            bb::U64 occ = occupancies[i];
            std::size_t idx = (occ * magic) >> (64 - bit_idx);

            if (!seen.contains(idx)) {
                seen.insert({idx, ray_attacks[i]});
                continue;
            }

            if (seen[idx] != ray_attacks[i]) {
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
            attacks[pos][idx] = ray_attacks[i];
        }
        return magic;
    }
}

std::array<bb::GMagic, 64>
get_bishop_gmagic(std::array<std::array<bb::U64, bb::BISHOP_ATTACK_ARR_SIZE>, 64> &attacks) {
    std::array<bb::GMagic, 64> bishop_tbl{};
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
        bishop_tbl[i].magic = find_magic(mask, bb::BISHOP_IDX_BITS, i, seed, attacks, bishop_rays);
    }
    return bishop_tbl;
}

std::array<bb::GMagic, 64>
get_rook_gmagic(std::array<std::array<bb::U64, bb::ROOK_ATTACK_ARR_SIZE>, 64> &attacks) {
    std::array<bb::GMagic, 64> rook_tbl{};
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
        rook_tbl[i].magic = find_magic(mask, bb::ROOK_IDX_BITS, i, seed, attacks, rook_rays);
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

template <typename std::size_t size>
void out_U64_arr(const std::array<bb::U64, size> &arr, std::string_view sv_name) {
    std::cout << "constexpr std::array<U64, " << arr.size() << "> " << sv_name << " = {\n";
    for (size_t i = 0; i < arr.size(); i++) {
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
    for (size_t i = 0; i < 64; i++) {
        std::cout << "    bb::GMagic{" << tbl[i].mask << "ULL, " << tbl[i].magic << "ULL}";
        if (i < 63) {
            std::cout << ",";
        }
        std::cout << "\n";
    }
    std::cout << "};\n";
}

template <std::size_t size>
void out_slide_attack(
    const std::array<std::array<bb::U64, size>, 64> &arr, std::string_view sv_name
) {
    std::cout << "constexpr std::array<std::array<bb::U64, " << size << ">, 64> " << sv_name
              << " = {\n";
    for (size_t i = 0; i < arr.size(); i++) {
        std::cout << "    std::array<U64, " << size << ">{\n";
        for (size_t j = 0; j < size; j++) {
            std::cout << "        " << arr[i][j] << "ULL";
            if (j < size - 1) {
                std::cout << ",";
            }
            std::cout << "\n";
        }
        std::cout << "    }";
        if (i < arr.size() - 1) {
            std::cout << ",";
        }
        std::cout << '\n';
    }
    std::cout << "};\n";
}

int main() {
    auto pawn_attacks = get_pawn_attacks();
    auto knight_attacks = get_knight_attacks();
    std::array<std::array<bb::U64, bb::BISHOP_ATTACK_ARR_SIZE>, 64> bishop_attacks;
    std::array<bb::GMagic, 64> bishop_tbl = get_bishop_gmagic(bishop_attacks);
    std::array<std::array<bb::U64, bb::ROOK_ATTACK_ARR_SIZE>, 64> rook_attacks;
    std::array<bb::GMagic, 64> rook_tbl = get_rook_gmagic(rook_attacks);
    out_color_arr(pawn_attacks, "pawn_attacks");
    out_U64_arr(knight_attacks, "knight_attacks");
    out_gmagic_arr(bishop_tbl, "bishop_tbl");
    out_gmagic_arr(rook_tbl, "rook_tbl");
    out_slide_attack(bishop_attacks, "bishop_attacks");
    out_slide_attack(rook_attacks, "rook_attacks");
}
