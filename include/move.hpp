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
    Move(sqr_t from_, sqr_t to_)
        : from(from_), to(to_), flag(MoveFlag::quiet), promotion_type(PieceType::none) {};
    Move(sqr_t from_, sqr_t to_, uint8_t flag_)
        : from(from_), to(to_), flag(flag_), promotion_type(PieceType::none) {};
    Move(sqr_t from_, sqr_t to_, uint8_t flag_, PieceType type)
        : from(from_), to(to_), flag(flag_), promotion_type(type) {};

    std::string format() const {
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
    constexpr static std::size_t fixed_cap = 256;
    Move data[fixed_cap];
    std::size_t len = 0;

  public:
    using iter = Move *;
    using citer = const Move *;

  public:
    MoveStack() = default;

    constexpr Move &operator[](std::size_t idx) {
        assert(idx < len);
        return data[idx];
    }
    constexpr const Move &operator[](std::size_t idx) const {
        assert(idx < len);
        return data[idx];
    }

    constexpr bool is_empty() const { return len == 0; }

    void push(const Move &move) {
        assert(len < fixed_cap);
        data[len] = move;
        len++;
    }
    void emplace(
        sqr_t from = -1, sqr_t to = -1, uint8_t flag = MoveFlag::quiet,
        PieceType promotion = PieceType::none
    ) {
        assert(len < fixed_cap);
        data[len].from = from;
        data[len].to = to;
        data[len].flag = flag;
        data[len].promotion_type = promotion;
        len++;
    }
    void pop() {
        assert(!is_empty());
        len--;
    }

    constexpr chess::Move &back() {
        assert(!is_empty());
        return data[len - 1];
    }
    constexpr const chess::Move &back() const {
        assert(!is_empty());
        return data[len - 1];
    }

    constexpr std::size_t size() const { return len; }

    iter begin() { return data; }
    iter end() { return data + len; }

    constexpr citer begin() const { return data; }
    constexpr citer end() const { return data + len; }
};
} // namespace chess
