#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <memory>

namespace chess {
template <uint8_t size_mb, typename T> class TransposTable {
  private:
    struct item {
        uint64_t hash = 0;
        T value;
    };
    const std::size_t m_capacity;
    std::size_t m_length = 0;
    std::unique_ptr<item[]> m_entries;

  private:
    static constexpr std::size_t capacity_for_mb() {
        const std::size_t bytes = static_cast<std::size_t>(size_mb) * 1'000'000;
        std::size_t capacity = 1;
        while (capacity < bytes / (2 * sizeof(item))) {
            capacity *= 2;
        }
        return capacity;
    }

  public:
    TransposTable()
        : m_capacity(capacity_for_mb()), m_entries(std::make_unique<item[]>(m_capacity)) {}

    class Iterator {
      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = item;
        using difference_type = std::ptrdiff_t;
        using pointer = item *;
        using reference = item &;

      private:
        pointer m_ptr;
        pointer m_end;

      private:
        void skip_empty() {
            while (m_ptr != m_end && m_ptr->hash == 0) {
                m_ptr++;
            }
        }

      public:
        Iterator(pointer ptr, pointer end) : m_ptr(ptr), m_end(end) { skip_empty(); }
        reference operator*() const { return *m_ptr; }
        pointer operator->() const { return m_ptr; }

        Iterator &operator++() {
            ++m_ptr;
            skip_empty();
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const Iterator &other) const { return m_ptr == other.m_ptr; }

        bool operator!=(const Iterator &other) const { return !(*this == other); }
    };

    class ConstIterator {
      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = const item;
        using difference_type = std::ptrdiff_t;
        using pointer = const item *;
        using reference = const item &;

      private:
        pointer m_ptr;
        pointer m_end;

      private:
        void skip_empty() {
            while (m_ptr != m_end && m_ptr->hash == 0) {
                m_ptr++;
            }
        }

      public:
        ConstIterator(pointer ptr, pointer end) : m_ptr(ptr), m_end(end) { skip_empty(); }
        reference operator*() const { return *m_ptr; }
        pointer operator->() const { return m_ptr; }

        ConstIterator &operator++() {
            ++m_ptr;
            skip_empty();
            return *this;
        }

        ConstIterator operator++(int) {
            ConstIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const ConstIterator &other) const { return m_ptr == other.m_ptr; }

        bool operator!=(const ConstIterator &other) const { return !(*this == other); }
    };

    Iterator begin() { return Iterator(m_entries.get(), m_entries.get() + m_capacity); }
    Iterator end() { return Iterator(m_entries.get() + m_capacity, m_entries.get() + m_capacity); }
    ConstIterator begin() const {
        return ConstIterator(m_entries.get(), m_entries.get() + m_capacity);
    }
    ConstIterator end() const {
        return ConstIterator(m_entries.get() + m_capacity, m_entries.get() + m_capacity);
    }

    Iterator at(uint64_t hash) {
        std::size_t index = hash & (m_capacity - 1);
        while (m_entries[index].hash != 0) {
            if (m_entries[index].hash == hash) {
                return Iterator(&m_entries[index], m_entries.get() + m_capacity);
            }
            index++;
            if (index >= m_capacity) {
                index = 0;
            }
        }
        return end();
    }
    ConstIterator at(uint64_t hash) const {
        std::size_t index = hash & (m_capacity - 1);
        while (m_entries[index].hash != 0) {
            if (m_entries[index].hash == hash) {
                return ConstIterator(&m_entries[index], m_entries.get() + m_capacity);
            }
            index++;
            if (index >= m_capacity) {
                index = 0;
            }
        }
        return end();
    }

    Iterator set(uint64_t hash, const T &value) {
        assert(hash != 0);

        std::size_t index = hash & (m_capacity - 1);
        while (m_entries[index].hash != 0) {
            if (m_entries[index].hash == hash) {
                m_entries[index].value = value;
                return Iterator(&m_entries[index], m_entries.get() + m_capacity);
            }
            index++;
            if (index >= m_capacity) {
                index = 0;
            }
        }
        m_length++;
        m_entries[index].hash = hash;
        m_entries[index].value = value;
        return Iterator(&m_entries[index], m_entries.get() + m_capacity);
    }

    std::size_t len() const { return m_length; }
};

template <uint8_t size_mb> class PerftTT {
    struct Entry {
        uint64_t hash = 0;
        uint64_t nodes = 0;
        uint8_t depth = 0;
    };

    static constexpr std::size_t capacity_for_mb() {
        const std::size_t bytes = static_cast<std::size_t>(size_mb) * 1'000'000;
        std::size_t capacity = 1;
        while (capacity < bytes / (2 * sizeof(Entry))) {
            capacity *= 2;
        }
        return capacity;
    }
    static constexpr std::size_t m_capacity = capacity_for_mb();

    std::unique_ptr<Entry[]> entries = std::make_unique<Entry[]>(m_capacity);

  public:
    PerftTT() { std::cout << "capacity: " << m_capacity << '\n'; };
    uint64_t *get(uint64_t hash, uint8_t depth) {
        std::size_t index = hash & (m_capacity - 1);
        Entry &e = entries[index];
        if (e.hash == hash && e.depth == depth) {
            return &e.nodes;
        }
        return nullptr;
    }

    void set(uint64_t hash, uint8_t depth, uint64_t nodes) {
        std::size_t index = hash & (m_capacity - 1);

        Entry &e = entries[index];
        e.hash = hash;
        e.depth = depth;
        e.nodes = nodes;
    }
};
} // namespace chess
