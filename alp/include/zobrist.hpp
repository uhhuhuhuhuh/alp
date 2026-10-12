#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "misc.hpp"
#include "piece_color.hpp"
#include "position.hpp"

namespace chess::zobrist {
constexpr std::size_t map_black_active_idx = 64 * (max_PieceType * 2);
constexpr std::size_t map_castle_rights_idx = map_black_active_idx + 1;
constexpr std::size_t map_ep_files_idx = map_castle_rights_idx + 4;
constexpr std::size_t map_size = map_ep_files_idx + 8;

constexpr uint64_t constexpr_rand64(uint64_t &seed) {
    seed ^= seed >> 12, seed ^= seed << 25, seed ^= seed >> 27;
    return seed * 2685821657736338717ULL;
}
constexpr std::array<uint64_t, map_size> constexpr_get_map() {
    std::array<uint64_t, map_size> zobrist_map{};
    uint64_t seed = 1070372;
    for (uint64_t &v : zobrist_map) {
        v = constexpr_rand64(seed);
    }
    return zobrist_map;
}
constexpr std::array<uint64_t, map_size> zobrist_map = constexpr_get_map();

constexpr uint64_t side() { return zobrist_map[map_black_active_idx]; }

inline constexpr uint64_t piece(PieceType type, Color color, sqr_t sqr) {
    constexpr uint8_t color_type_max = max_PieceType * 2;
    std::size_t idx = (sqr * color_type_max) + (color_type_max * static_cast<uint8_t>(color)) +
                      static_cast<std::size_t>(type) - 1;
    return zobrist_map[idx];
}

inline constexpr uint64_t castle_rights(uint8_t rights) {
    uint64_t hash = 0ULL;
    while (rights > 0) {
        int idx = lsb(rights);
        rights &= rights - 1;

        hash ^= zobrist_map[map_castle_rights_idx + idx];
    }
    return hash;
}

inline constexpr uint64_t ep(sqr_t ep) {
    if (ep == State::ep_none) {
        return 0;
    }
    file_t ep_file = ep % 8;
    return zobrist_map[map_ep_files_idx + ep_file];
}

inline uint64_t get_hash(const Position &pos) {
    uint64_t hash = 0ULL;

    bb::U64 occ = pos.bb.occupancy();
    while (occ > 0) {
        std::size_t i = lsb(occ);
        occ &= occ - 1;

        PieceType type = pos.board[i].type;
        if (type == PieceType::none) {
            continue;
        }
        hash ^= piece(type, pos.board[i].color, i);
    }

    if (pos.st->active_color == Color::black) {
        hash ^= side();
    }

    hash ^= castle_rights(pos.st->castle_rights);
    hash ^= ep(pos.st->ep_sqr);

    return hash;
}
} // namespace chess::zobrist
