#pragma once

#include "square.hpp"
#include <cassert>
#include <cstdint>

namespace chess {
enum MoveFlag : uint8_t {
    quiet,
    capture,
    ep,
    ep_victim,
    castle,
    promotion,
};
struct Move {
  public:
    sqr_t from;
    sqr_t to;
    uint8_t flags;

  public:
    Move() : from(-1), to(-1), flags(MoveFlag::quiet) {};
    Move(sqr_t from_, sqr_t to_) : from(from_), to(to_), flags(MoveFlag::quiet) {};
    Move(sqr_t from_, sqr_t to_, uint8_t flags_) : from(from_), to(to_), flags(flags_) {};
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

class MoveArray {
  private:
    constexpr static std::size_t move_array_len = 36;
    constexpr static float cap_multiplier = 1.5;
    Move fixed[move_array_len];
    Move *data = fixed;
    std::size_t len = 0;
    std::size_t cap = 0;
    bool dynamic_array = false;

    inline void resize() {
        std::size_t new_cap = cap == 0 ? len : cap;
        new_cap *= cap_multiplier;
        Move *new_buf = new Move[new_cap];
        Move *old_buf = data;
        for (std::size_t i = 0; i < len; i++, old_buf++) {
            new_buf[i] = *old_buf;
        }
        if (dynamic_array) {
            delete[] data;
        }
        data = new_buf;
        dynamic_array = true;
        cap = new_cap;
    }

  public:
    inline ~MoveArray() {
        if (dynamic_array) {
            delete[] data;
        }
    }

    inline constexpr Move &operator[](std::size_t idx) {
        assert(idx < len);
        return data[idx];
    }
    inline constexpr const Move &operator[](std::size_t idx) const {
        assert(idx < len);
        return data[idx];
    }

    inline constexpr bool will_resize() {
        if (dynamic_array) {
            return len + 1 > cap;
        }
        return len + 1 > move_array_len;
    }

    inline constexpr void push_back(const Move &move) {
        if (will_resize()) {
            resize();
        }
        data[len] = move;
        len++;
    }
    inline constexpr chess::Move &
    emplace_back(sqr_t from = -1, sqr_t to = -1, uint8_t flags = MoveFlag::quiet) {
        if (will_resize()) {
            resize();
        }
        data[len].from = from;
        data[len].to = to;
        data[len].flags = flags;
        len++;
        return data[len - 1];
    }
    inline constexpr void pop_back() {
        assert(len > 0);
        len--;
    }

    inline constexpr chess::Move &back() {
        assert(len > 0);
        return data[len - 1];
    }
    inline constexpr const chess::Move &back() const {
        assert(len > 0);
        return data[len - 1];
    }

    inline constexpr std::size_t size() const { return len; }

    inline MoveArrayIterator begin() { return MoveArrayIterator(&data[0]); }
    inline MoveArrayIterator end() { return MoveArrayIterator(&data[len]); }
};
} // namespace chess
