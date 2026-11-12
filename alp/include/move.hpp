#pragma once

#include "piece_color.hpp"
#include "square.hpp"

#include <cassert>
#include <cstdint>
#include <format>
#include <string_view>

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
    constexpr Move() = default;
    constexpr Move(sqr_t from_, sqr_t to_)
        : from(from_), to(to_), flag(MoveFlag::quiet), promotion_type(PieceType::none) {};
    constexpr Move(sqr_t from_, sqr_t to_, uint8_t flag_)
        : from(from_), to(to_), flag(flag_), promotion_type(PieceType::none) {};
    constexpr Move(sqr_t from_, sqr_t to_, uint8_t flag_, PieceType promotion_type_)
        : from(from_), to(to_), flag(flag_), promotion_type(promotion_type_) {};

    static constexpr Move none() { return Move{0, 0, MoveFlag::quiet, PieceType::none}; };

    bool operator==(const chess::Move &other) const {
        return from == other.from && to == other.to && flag == other.flag &&
               promotion_type == other.promotion_type;
    }
    bool operator!=(const chess::Move &other) const { return !(*this == other); }

    // compare everything other then the move flags
    bool weak_cmp(const chess::Move &other) const {
        return from == other.from && to == other.to && promotion_type == other.promotion_type;
    }

    // returns false if the move is invalid
    // returns true if the move is valid
    bool parse_uci(std::string_view sv) {
        if (sv.size() != 4 && sv.size() != 5) { return false; }

        auto sqr = [](auto s) -> sqr_t {
            return (s[0] >= 'a' && s[0] <= 'h' && s[1] >= '1' && s[1] <= '8')
                       ? s[0] - 'a' + 8 * (s[1] - '1')
                       : -1;
        };
        sqr_t f = sqr(sv.substr(0, 2));
        sqr_t t = sqr(sv.substr(2, 2));

        if (f < 0 || t < 0) { return false; }

        from = f;
        to = t;

        if (sv.size() == 4) {
            promotion_type = PieceType::none;
            return true;
        }

        Piece piece(sv[4]);
        switch (piece.type) {
        case PieceType::knight:
        case PieceType::bishop:
        case PieceType::rook:
        case PieceType::queen:  promotion_type = piece.type; return true;

        default: return false;
        }
    }

    std::string format_uci() const {
        std::string formated = std::format("{}{}", format_sqr(from), format_sqr(to));
        if (promotion_type == PieceType::none) { return formated; }
        return formated += Piece(promotion_type, Color::black).format();
    };
};

template <typename T = Move, std::size_t cap = 256> class MoveStack {
  private:
    T data[cap];
    std::size_t len = 0;

  public:
    using iter = T *;
    using citer = const T *;

  public:
    MoveStack() = default;
    MoveStack(const MoveStack &other) {
        len = other.len;
        for (std::size_t i = 0; i < len; i++) { data[i] = other[i]; }
    };

    MoveStack &operator=(const MoveStack &other) {
        len = other.len;
        for (std::size_t i = 0; i < len; i++) { data[i] = other[i]; }
        return *this;
    };

    constexpr T &operator[](std::size_t idx) {
        assert(idx < len);
        return data[idx];
    }
    constexpr const T &operator[](std::size_t idx) const {
        assert(idx < len);
        return data[idx];
    }

    constexpr bool is_empty() const { return len == 0; }

    void push(const T &move) {
        assert(len < cap);
        data[len] = move;
        len++;
    }
    template <typename... Args> void emplace(Args &&...args) {
        assert(len < cap);
        data[len] = T{std::forward<Args>(args)...};
        len++;
    }
    void pop() {
        assert(!is_empty());
        len--;
    }

    void clear() { len = 0; }

    constexpr T &back() {
        assert(!is_empty());
        return data[len - 1];
    }
    constexpr const T &back() const {
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
