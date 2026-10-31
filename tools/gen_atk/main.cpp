#include "constbb.hpp"

#include <iostream>
#include <vector>

using namespace chess;

bb::U64 xorshift64star(uint64_t &x) {
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    return x * 0x2545F4914F6CDD1DULL;
}

template <typename ray_t, std::size_t size>
inline bb::U64 find_magic(
    bb::U64 mask, bb::U64 bit_idx, sqr_t sqr, bb::U64 &seed, bb::OffsetAttacks<size> &attacks,
    ray_t ray, size_t &offset
) {
    const std::size_t table_size = 1ULL << bit_idx;
    std::vector<bb::U64> occupancies;
    std::vector<bb::U64> ray_attacks;
    occupancies.reserve(table_size);
    ray_attacks.reserve(table_size);

    for (bb::U64 subset = mask;; subset = (subset - 1) & mask) {
        occupancies.push_back(subset);
        ray_attacks.push_back(ray(subset, sqr));
        if (subset == 0) { break; }
    }

    std::vector<bb::SeenAttackBB> attack_hist(table_size);
    for (uint32_t gen = 0;; gen++) {
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

        if (!valid) { continue; }
        for (std::size_t i = 0; i < occupancies.size(); i++) {
            bb::U64 occ = occupancies[i];
            std::size_t idx = (occ * magic) >> (64 - bit_idx);
            attacks.data[offset + idx] = ray_attacks[i];
        }
        attacks.offsets[sqr] = offset;
        offset += 1ULL << bit_idx;
        return magic;
    }
}

inline bb::U64 bishop_rays(bb::U64 occ, sqr_t sqr) {
    bb::U64 rays = 0ULL;

    rank_t t_rank = sqr / 8;
    file_t t_file = sqr % 8;
    rank_t rank = 0;
    file_t file = 0;

    for (rank = t_rank + 1, file = t_file + 1; rank < 8 && file < 8; rank++, file++) {
        bb::U64 at_bb = bb::bb((rank * 8) + file);
        rays |= at_bb;
        if ((occ & at_bb) != 0) break;
    }
    for (rank = t_rank + 1, file = t_file - 1; rank < 8 && file >= 0; rank++, file--) {
        bb::U64 at_bb = bb::bb((rank * 8) + file);
        rays |= at_bb;
        if ((occ & at_bb) != 0) break;
    }
    for (rank = t_rank - 1, file = t_file + 1; rank >= 0 && file < 8; rank--, file++) {
        bb::U64 at_bb = bb::bb((rank * 8) + file);
        rays |= at_bb;
        if ((occ & at_bb) != 0) break;
    }
    for (rank = t_rank - 1, file = t_file - 1; rank >= 0 && file >= 0; rank--, file--) {
        bb::U64 at_bb = bb::bb((rank * 8) + file);
        rays |= at_bb;
        if ((occ & at_bb) != 0) break;
    }

    return rays;
}

inline bb::U64 rook_rays(bb::U64 occ, sqr_t sqr) {
    bb::U64 rays = 0ULL;

    rank_t t_rank = sqr / 8;
    file_t t_file = sqr % 8;
    rank_t rank = 0;
    file_t file = 0;

    for (rank = t_rank + 1; rank < 8; rank++) {
        bb::U64 at_bb = bb::bb((rank * 8) + t_file);
        rays |= at_bb;
        if ((occ & at_bb) != 0) break;
    }
    for (rank = t_rank - 1; rank >= 0; rank--) {
        bb::U64 at_bb = bb::bb((rank * 8) + t_file);
        rays |= at_bb;
        if ((occ & at_bb) != 0) break;
    }
    for (file = t_file + 1; file < 8; file++) {
        bb::U64 at_bb = bb::bb((t_rank * 8) + file);
        rays |= at_bb;
        if ((occ & at_bb) != 0) break;
    }
    for (file = t_file - 1; file >= 0; file--) {
        bb::U64 at_bb = bb::bb((t_rank * 8) + file);
        rays |= at_bb;
        if ((occ & at_bb) != 0) break;
    }

    return rays;
}

inline bb::AttacksTbl<bb::bishop_attacks_size> get_bishop_attack_tbl() {
    bb::AttacksTbl<bb::bishop_attacks_size> attack_tbl;
    std::size_t offset = 0;
    // a random starting seed
    uint64_t seed = 578;

    for (std::size_t i = 0; i < 64; i++) {
        bb::U64 mask = 0ULL;

        rank_t r;
        file_t f;
        // t stands for target
        rank_t tr = i / 8;
        file_t tf = i % 8;
        for (r = tr + 1, f = tf + 1; r < 7 && f < 7; r++, f++) mask |= bb::bb(r * 8 + f);
        for (r = tr + 1, f = tf - 1; r < 7 && f > 0; r++, f--) mask |= bb::bb(r * 8 + f);
        for (r = tr - 1, f = tf + 1; r > 0 && f < 7; r--, f++) mask |= bb::bb(r * 8 + f);
        for (r = tr - 1, f = tf - 1; r > 0 && f > 0; r--, f--) mask |= bb::bb(r * 8 + f);

        attack_tbl.gmagic[i].mask = mask;
        attack_tbl.gmagic[i].magic =
            find_magic(mask, bb::bishop_bit_idxs[i], i, seed, attack_tbl.atks, bishop_rays, offset);
    }
    return attack_tbl;
}

inline bb::AttacksTbl<bb::rook_attacks_size> get_rook_attack_tbl() {
    bb::AttacksTbl<bb::rook_attacks_size> attack_tbl;
    std::size_t offset = 0;
    // a random starting seed
    uint64_t seed = 109;

    for (std::size_t i = 0; i < 64; i++) {
        bb::U64 mask = 0LL;
        // t stands for target
        rank_t t_rank = i / 8;
        rank_t t_file = i % 8;
        for (rank_t rank = t_rank + 1; rank < 7; rank++) mask |= bb::bb(rank * 8 + t_file);
        for (rank_t rank = t_rank - 1; rank > 0; rank--) mask |= bb::bb(rank * 8 + t_file);
        for (file_t file = t_file + 1; file < 7; file++) mask |= bb::bb(t_rank * 8 + file);
        for (file_t file = t_file - 1; file > 0; file--) mask |= bb::bb(t_rank * 8 + file);

        attack_tbl.gmagic[i].mask = mask;
        attack_tbl.gmagic[i].magic =
            find_magic(mask, bb::rook_bit_idxs[i], i, seed, attack_tbl.atks, rook_rays, offset);
    }

    return attack_tbl;
}

int main() {
    std::cout << "constexpr AttacksTbl<bishop_attacks_size> bishop_atk_tbl = ";
    get_bishop_attack_tbl().output(std::cout);
    std::cout << "constexpr AttacksTbl<rook_attacks_size> rook_atk_tbl = ";
    get_rook_attack_tbl().output(std::cout);
}
