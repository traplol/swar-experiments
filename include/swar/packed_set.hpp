#pragma once

#include "packed_word.hpp"
#include <array>

namespace swar {

/// A fixed-capacity set of N-bit integers, stored as a compile-time-sized
/// array of PackedWord<N> instances. Capacity is the maximum number of
/// elements the set can hold.
///
/// Each word holds up to PackedWord<N>::lanes elements. Empty lanes are
/// represented by zero, so stored values must be in [1, max_safe_value].
///
/// This is a simple flat container — not hash-based, not sorted.
/// Suitable for small sets where SWAR search is fast enough.
template <unsigned N, std::size_t Capacity>
class PackedSet {
    static_assert(Capacity > 0, "Capacity must be > 0");

  public:
    using Word = PackedWord<N>;
    static constexpr unsigned lanes_per_word = Word::lanes;
    static constexpr std::size_t num_words =
        (Capacity + lanes_per_word - 1) / lanes_per_word;
    static constexpr std::size_t capacity = Capacity;

    constexpr PackedSet() noexcept : words_{} {}

    /// Insert a value into the set. Returns true if inserted,
    /// false if already present or full.
    /// v must be in [1, Word::max_safe_value] (0 is reserved as "empty").
    bool insert(uint64_t v) {
        assert(v >= 1 && v <= Word::max_safe_value);
        if (contains(v))
            return false;
        for (auto &w : words_) {
            int idx = w.find_zero();
            if (idx >= 0) {
                w = w.set(static_cast<unsigned>(idx), v);
                return true;
            }
        }
        // All words full — set is at capacity
        return false;
    }

    /// Remove a value from the set. Returns true if it was present.
    bool erase(uint64_t v) {
        assert(v >= 1 && v <= Word::max_safe_value);
        for (auto &w : words_) {
            int idx = w.find(v);
            if (idx >= 0) {
                w = w.set(static_cast<unsigned>(idx), 0);
                return true;
            }
        }
        return false;
    }

    /// Check if the set contains value v.
    bool contains(uint64_t v) const {
        assert(v >= 1 && v <= Word::max_safe_value);
        for (const auto &w : words_) {
            if (w.contains(v))
                return true;
        }
        return false;
    }

    /// Fixed capacity of the set.
    static constexpr std::size_t size() noexcept { return capacity; }

    /// Number of PackedWords backing this set.
    static constexpr std::size_t word_count() noexcept { return num_words; }

    /// Direct access to underlying words (for inspection / benchmarking).
    const std::array<Word, num_words> &words() const noexcept {
        return words_;
    }

  private:
    std::array<Word, num_words> words_;
};

} // namespace swar
