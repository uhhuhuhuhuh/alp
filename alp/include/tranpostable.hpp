#pragma once

#include "move.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace chess {
enum class TTBound : uint8_t { exact, lower, upper };

struct TTentry {
    uint64_t key = 0;
    int16_t depth = 0;
    int32_t score = 0;
    TTBound bound = TTBound::exact;
    Move best_move{};
};

struct TTbucket {
    static constexpr std::size_t len = 4;
    std::array<TTentry, len> data{};
};

template <uint8_t size_mb> class TransposTable {
  private:
    static_assert(size_mb > 0, "Transposition table size must be greater than 0 MB");

    static constexpr std::size_t capacity_for_mb() {
        constexpr std::size_t bytes = static_cast<std::size_t>(size_mb) * 1'000'000;

        std::size_t capacity = 1;
        while ((capacity * 2) * sizeof(TTbucket) <= bytes) {
            capacity *= 2;
        }

        return capacity;
    }
    static constexpr std::size_t m_capacity = capacity_for_mb();

    std::unique_ptr<TTbucket[]> m_buckets;
    std::size_t m_length = 0;

  public:
    TransposTable() : m_buckets(std::make_unique<TTbucket[]>(m_capacity)) {}

    TTentry *probe(uint64_t hash) {
        assert(hash != 0);

        const std::size_t index = hash & (m_capacity - 1);
        TTbucket &bucket = m_buckets[index];

        for (TTentry &entry : bucket.data) {
            if (entry.key == hash) {
                return &entry;
            }
        }

        return nullptr;
    }

    const TTentry *probe(uint64_t hash) const {
        assert(hash != 0);

        const std::size_t index = hash & (m_capacity - 1);
        const TTbucket &bucket = m_buckets[index];
        for (const TTentry &entry : bucket.data) {
            if (entry.key == hash) {
                return &entry;
            }
        }

        return nullptr;
    }

    TTentry *set(uint64_t hash, const TTentry &value) {
        assert(hash != 0);
        assert(value.key == hash);

        const std::size_t index = hash & (m_capacity - 1);
        TTbucket &bucket = m_buckets[index];
        for (TTentry &entry : bucket.data) {
            if (entry.key == hash) {
                entry = value;
                return &entry;
            }
        }

        for (TTentry &entry : bucket.data) {
            if (entry.key == 0) {
                entry = value;
                m_length++;
                return &entry;
            }
        }

        TTentry *replacement = &bucket.data[0];
        for (TTentry &entry : bucket.data) {
            if (entry.depth < replacement->depth) {
                replacement = &entry;
            }
        }
        *replacement = value;
        return replacement;
    }

    std::size_t len() const { return m_length; }

    static constexpr std::size_t capacity() { return m_capacity; }
    static constexpr std::size_t entry_capacity() { return m_capacity * TTbucket::len; }
};
} // namespace chess
