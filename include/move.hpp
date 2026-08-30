#pragma once

#include "piece.hpp"
#include "square.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>

namespace chess {
enum MoveFlag : uint8_t {
    quiet,
    capture,
    ep,
    ep_victim,
    castle,
};
struct Move {
  public:
    sqr_t from;
    sqr_t to;
    uint8_t flag;
    PieceType promotion_type;

  public:
    Move() = default;
    // Move() : from(-1), to(-1), flags(MoveFlag::quiet), promotion_type(PieceType::none) {};
    Move(sqr_t from_, sqr_t to_)
        : from(from_), to(to_), flag(MoveFlag::quiet), promotion_type(PieceType::none) {};
    Move(sqr_t from_, sqr_t to_, uint8_t flag_)
        : from(from_), to(to_), flag(flag_), promotion_type(PieceType::none) {};

    inline std::string format() const {
        std::string formated;
        formated += sqr_format(from);
        formated += sqr_format(to);
        if (promotion_type != PieceType::none) {
            formated += '=';
            formated += Piece(promotion_type, Color::white).format();
        }
        return formated;
    };
};

class MoveStack {
  private:
    constexpr static std::size_t fixed_cap = 128;
    constexpr static std::size_t cap_multiplier = 2;
    Move fixed[fixed_cap];
    Move *data = fixed;
    std::size_t len = 0;
    std::size_t cap = fixed_cap;

    inline void resize() {
        std::size_t new_cap = cap * cap_multiplier;
        Move *new_buf = new Move[new_cap];
        std::memcpy(new_buf, data, len * sizeof(Move));
        if (data != fixed) {
            delete[] data;
        }
        data = new_buf;
        cap = new_cap;
    }

  public:
    using iter = Move *;
    using citer = const Move *;

  public:
    MoveStack() = default;
    inline ~MoveStack() {
        if (data != fixed) {
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

    inline constexpr bool will_resize() const { return len == cap; }

    inline void push(const Move &move) {
        if (will_resize()) {
            resize();
        }
        data[len] = move;
        len++;
    }
    inline void emplace(
        sqr_t from = -1, sqr_t to = -1, uint8_t flag = MoveFlag::quiet,
        PieceType promotion = PieceType::none
    ) {
        if (will_resize()) {
            resize();
        }
        data[len].from = from;
        data[len].to = to;
        data[len].flag = flag;
        data[len].promotion_type = promotion;
        len++;
    }
    inline void pop() {
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

    iter begin() { return data; }
    iter end() { return data + len; }

    citer begin() const { return data; }
    citer end() const { return data + len; }
};
} // namespace chess
