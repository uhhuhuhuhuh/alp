#pragma once

#include "square.hpp"
#include <cassert>

namespace chess {
enum MoveFlag {
    quiet,
    capture,
    ep,
    castle,
    promotion,
};
struct Move {
  public:
    sqr_t from;
    sqr_t to;
    int8_t flags;

  public:
    Move() : from(-1), to(-1), flags(MoveFlag::quiet) {};
    Move(sqr_t from_, sqr_t to_) : from(from_), to(to_), flags(MoveFlag::quiet) {};
};

struct MoveArrayIterator {
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = Move;
    using pointer = Move *;
    using reference = Move &;

    MoveArrayIterator(pointer ptr) : m_ptr(ptr) {}

    reference operator*() const { return *m_ptr; }
    pointer operator->() { return m_ptr; }

    MoveArrayIterator &operator++() {
        m_ptr++;
        return *this;
    }
    MoveArrayIterator operator++(int) {
        MoveArrayIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    friend bool operator==(const MoveArrayIterator &a, const MoveArrayIterator &b) {
        return a.m_ptr == b.m_ptr;
    };
    friend bool operator!=(const MoveArrayIterator &a, const MoveArrayIterator &b) {
        return a.m_ptr != b.m_ptr;
    };

  private:
    pointer m_ptr;
};

constexpr std::size_t move_array_size = 256;
class MoveArray {
  private:
    Move data[move_array_size];
    std::size_t len = 0;

  public:
    inline constexpr Move &operator[](std::size_t idx) {
        assert(idx < len);
        return data[idx];
    }
    inline constexpr const Move &operator[](std::size_t idx) const {
        assert(idx < len);
        return data[idx];
    }

    inline constexpr void push_back(const Move &move) {
        assert(len < move_array_size);
        data[len] = move;
        len++;
    }
    inline constexpr void pop_back() {
        assert(len > 0);
        len--;
    }

    inline constexpr std::size_t size() const { return len; }

    inline MoveArrayIterator begin() { return MoveArrayIterator(&data[0]); }
    inline MoveArrayIterator end() { return MoveArrayIterator(&data[len]); }
};
} // namespace chess
