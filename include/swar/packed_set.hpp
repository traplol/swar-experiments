#pragma once

#include "packed_word.hpp"
#include <vector>

namespace swar {

/// A dynamically-sized set of N-bit integers, stored as a sequence of
/// PackedWord<N> instances. Each word holds up to PackedWord<N>::lanes
/// elements. Empty lanes are represented by zero, so the stored values
/// must be in [1, max_safe_value] to distinguish "empty" from "present".
///
/// This is a simple flat container — not hash-based, not sorted.
/// Suitable for small-to-medium sets where SWAR search is fast enough.
template <unsigned N>
class PackedSet {
  public:
    using Word = PackedWord<N>;
    static constexpr unsigned lanes_per_word = Word::lanes;

    PackedSet() = default;

    /// Insert a value into the set. Returns true if inserted,
    /// false if already present.
    /// v must be in [1, Word::max_safe_value] (0 is reserved as "empty").
    bool insert(uint64_t v) {
        assert(v >= 1 && v <= Word::max_safe_value);
        // Check if already present
        if (contains(v))
            return false;
        // Find a word with a free (zero) lane
        for (auto &w : words_) {
            int idx = w.find_zero();
            if (idx >= 0) {
                w = w.set(static_cast<unsigned>(idx), v);
                ++size_;
                return true;
            }
        }
        // All words full — add a new one
        Word fresh;
        fresh = fresh.set(0, v);
        words_.push_back(fresh);
        ++size_;
        return true;
    }

    /// Remove a value from the set. Returns true if it was present.
    bool erase(uint64_t v) {
        assert(v >= 1 && v <= Word::max_safe_value);
        for (auto &w : words_) {
            int idx = w.find(v);
            if (idx >= 0) {
                w = w.set(static_cast<unsigned>(idx), 0);
                --size_;
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

    /// Number of elements in the set.
    std::size_t size() const noexcept { return size_; }

    /// True if the set is empty.
    bool empty() const noexcept { return size_ == 0; }

    /// Number of PackedWords in use.
    std::size_t word_count() const noexcept { return words_.size(); }

    /// Direct access to underlying words (for inspection / benchmarking).
    const std::vector<Word> &words() const noexcept { return words_; }

  private:
    std::vector<Word> words_;
    std::size_t size_ = 0;
};

} // namespace swar
