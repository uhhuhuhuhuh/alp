#pragma once

#include "bitboard.hpp"
#include "piece_color.hpp"
#include "square.hpp"

#include <cstdint>

namespace chess {
enum CastleRights : uint8_t {
    none = 0,
    white_king = 1,
    white_queen = 1 << 1,
    black_king = 1 << 2,
    black_queen = 1 << 3,
};

class State {
  public:
    static constexpr sqr_t ep_none = -1;

    uint64_t hash = 0ULL;
    Color active_color = Color::white;
    uint8_t castle_rights = CastleRights::none;
    Piece captured_piece;
    // set to ep_none if there is nothing to en passant
    sqr_t ep_sqr = ep_none;
    bb::U64 checkers = 0ULL;
    uint32_t halfmove_clock = 0;

    State *prev = nullptr;

  public:
    State() = default;
};

// a double ended queue
class StateStack {
  private:
    static constexpr std::size_t m_block_size = 256;

    std::size_t m_len;
    std::size_t m_total_cap;
    std::size_t m_block_count;
    chess::State m_stack_block[m_block_size];
    chess::State **m_blocks;

  private:
    void grow() {
        std::size_t new_block_count = m_block_count + 1;
        m_total_cap += m_block_size;
        chess::State **new_blocks = new chess::State *[new_block_count];
        for (std::size_t i = 0; i < m_block_count; ++i) {
            new_blocks[i] = m_blocks[i];
        }
        new_blocks[new_block_count - 1] = new chess::State[m_block_size];
        delete[] m_blocks;
        m_blocks = new_blocks;
        m_block_count = new_block_count;
    }

  public:
    StateStack() : m_len(0), m_block_count(1), m_total_cap(m_block_size), m_stack_block() {
        m_blocks = new chess::State *[m_block_count];
        // the first block will point to a stack allocated array
        m_blocks[0] = m_stack_block;
    };

    ~StateStack() {
        for (std::size_t i = 1 /* first is stack allocated */; i < m_block_count; i++) {
            delete[] m_blocks[i];
        }
        delete[] m_blocks;
    }

    StateStack(const StateStack &) = delete;
    void operator=(const StateStack &) = delete;

    chess::State &operator[](std::size_t idx) {
        assert(idx < m_len);
        return m_blocks[idx / m_block_size][idx % m_block_size];
    }
    const chess::State &operator[](std::size_t idx) const {
        assert(idx < m_len);
        return m_blocks[idx / m_block_size][idx % m_block_size];
    }

    chess::State &at(std::size_t idx) {
        assert(idx < m_len);
        return m_blocks[idx / m_block_size][idx % m_block_size];
    }
    const chess::State &at(std::size_t idx) const {
        assert(idx < m_len);
        return m_blocks[idx / m_block_size][idx % m_block_size];
    }

    chess::State &back() {
        assert(m_len > 0);
        return at(m_len - 1);
    }
    const chess::State &back() const {
        assert(m_len > 0);
        return at(m_len - 1);
    }

    std::size_t size() const { return m_len; }

    chess::State &push(const chess::State &st) {
        if (m_len >= m_total_cap) {
            grow();
        }
        m_len++;
        return at(m_len - 1) = st;
    }

    chess::State &emplace() { return push(chess::State{}); }

    void pop() {
        assert(m_len > 0);
        m_len--;
    }

    void clear() { m_len = 0; }
    void resize(std::size_t size) {
        assert(size <= m_len);
        m_len = size;
    }
};
} // namespace chess
