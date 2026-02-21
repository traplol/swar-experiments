#pragma once

#include <cassert>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace swar {

/// Compute the "broadcast one" constant at compile time:
/// a uint64_t with a 1 deposited at every N-bit lane position.
/// E.g. for N=5: bits 0,5,10,15,20,25,30,35,40,45,50,55 are set.
template <unsigned N>
constexpr uint64_t make_broadcast_one() {
    static_assert(N >= 1 && N <= 64, "N must be in [1,64]");
    uint64_t v = 0;
    for (unsigned i = 0; i < 64 / N; ++i)
        v |= uint64_t(1) << (i * N);
    return v;
}

/// Broadcast the lane mask (all bits set in one lane) across all lanes.
template <unsigned N>
constexpr uint64_t make_lane_mask_broadcast() {
    constexpr uint64_t lane_mask = (uint64_t(1) << N) - 1;
    uint64_t v = 0;
    for (unsigned i = 0; i < 64 / N; ++i)
        v |= lane_mask << (i * N);
    return v;
}

/// Broadcast the MSB of each lane across all lanes (for haszero detection).
template <unsigned N>
constexpr uint64_t make_high_bits() {
    uint64_t v = 0;
    for (unsigned i = 0; i < 64 / N; ++i)
        v |= uint64_t(1) << (i * N + (N - 1));
    return v;
}

/// A single machine word packing floor(64/N) values of N bits each.
///
/// N must be in [5, 14] for the intended use case, but the implementation
/// works for any N in [1, 32].
///
/// IMPORTANT: The haszero trick requires that the MSB of each lane is NOT
/// part of the value space. For contains/find/count_eq to work correctly
/// without false positives, values must be in [0, 2^(N-1) - 1] — i.e. the
/// top bit of each lane is reserved as a "guard bit". This means:
///   - For N=5, valid values are 0..15  (4 usable bits)
///   - For N=6, valid values are 0..31  (5 usable bits)
///   - etc.
///
/// If you don't need contains/find/count_eq, the full range [0, 2^N - 1]
/// can be used for get/set/broadcast.
template <unsigned N>
class PackedWord {
    static_assert(N >= 1 && N <= 32, "N must be in [1,32]");

  public:
    // ----- compile-time constants -----
    static constexpr unsigned bits = N;
    static constexpr unsigned lanes = 64 / N;
    static constexpr uint64_t lane_mask = (uint64_t(1) << N) - 1;
    static constexpr uint64_t broadcast_one = make_broadcast_one<N>();
    static constexpr uint64_t all_lanes_mask = make_lane_mask_broadcast<N>();
    static constexpr uint64_t high_bits = make_high_bits<N>();

    // Maximum value that can safely be used with contains/find (guard-bit safe)
    static constexpr uint64_t max_safe_value = (uint64_t(1) << (N - 1)) - 1;

    // ----- construction -----
    constexpr PackedWord() noexcept : word_(0) {}
    constexpr explicit PackedWord(uint64_t raw) noexcept : word_(raw) {}

    /// Raw access
    constexpr uint64_t raw() const noexcept { return word_; }

    // ----- broadcast -----

    /// Fill every lane with value v.
    static constexpr PackedWord broadcast(uint64_t v) noexcept {
        return PackedWord(v * broadcast_one);
    }

    // ----- lane access -----

    /// Extract the value in lane i (0-indexed from LSB).
    constexpr uint64_t get(unsigned i) const noexcept {
        assert(i < lanes);
        return (word_ >> (i * N)) & lane_mask;
    }

    /// Set lane i to value v, returning a new PackedWord.
    constexpr PackedWord set(unsigned i, uint64_t v) const noexcept {
        assert(i < lanes);
        assert(v <= lane_mask);
        uint64_t shift = i * N;
        uint64_t cleared = word_ & ~(lane_mask << shift);
        return PackedWord(cleared | (v << shift));
    }

    // ----- SWAR search operations -----
    // These require values to have their MSB clear (guard bit = 0).

    /// Returns a mask word with the MSB of each lane set if that lane is zero.
    /// Implements: haszero(x) = (x - ones) & ~x & high_bits
    constexpr uint64_t zero_lanes_mask() const noexcept {
        return (word_ - broadcast_one) & ~word_ & high_bits;
    }

    /// True if any lane equals v.
    /// v must be <= max_safe_value (guard bit must be 0).
    constexpr bool contains(uint64_t v) const noexcept {
        assert(v <= max_safe_value);
        PackedWord xored(word_ ^ broadcast(v).raw());
        return xored.zero_lanes_mask() != 0;
    }

    /// Return the index of the first lane equal to v, or -1 if not found.
    /// v must be <= max_safe_value.
    constexpr int find(uint64_t v) const noexcept {
        assert(v <= max_safe_value);
        uint64_t mask = PackedWord(word_ ^ broadcast(v).raw()).zero_lanes_mask();
        if (mask == 0)
            return -1;
        // The set bit is at position (lane_index * N + N - 1).
        unsigned bit_pos = __builtin_ctzll(mask);
        return static_cast<int>(bit_pos / N);
    }

    /// Return the index of the first lane equal to zero, or -1 if none.
    constexpr int find_zero() const noexcept {
        uint64_t mask = zero_lanes_mask();
        if (mask == 0)
            return -1;
        unsigned bit_pos = __builtin_ctzll(mask);
        return static_cast<int>(bit_pos / N);
    }

    /// Count how many lanes equal v.
    /// v must be <= max_safe_value.
    constexpr unsigned count_eq(uint64_t v) const noexcept {
        assert(v <= max_safe_value);
        uint64_t mask = PackedWord(word_ ^ broadcast(v).raw()).zero_lanes_mask();
        return static_cast<unsigned>(__builtin_popcountll(mask));
    }

    // ----- min / max (iterative — simple and correct) -----

    /// Minimum value across all occupied lanes.
    /// `count` is the number of valid lanes (from LSB). Defaults to all lanes.
    constexpr uint64_t min(unsigned count) const noexcept {
        assert(count > 0 && count <= lanes);
        uint64_t m = get(0);
        for (unsigned i = 1; i < count; ++i) {
            uint64_t v = get(i);
            if (v < m)
                m = v;
        }
        return m;
    }

    constexpr uint64_t min() const noexcept { return min(lanes); }

    /// Maximum value across all occupied lanes.
    constexpr uint64_t max(unsigned count) const noexcept {
        assert(count > 0 && count <= lanes);
        uint64_t m = get(0);
        for (unsigned i = 1; i < count; ++i) {
            uint64_t v = get(i);
            if (v > m)
                m = v;
        }
        return m;
    }

    constexpr uint64_t max() const noexcept { return max(lanes); }

    // ----- comparison -----
    constexpr bool operator==(const PackedWord &o) const noexcept {
        return word_ == o.word_;
    }
    constexpr bool operator!=(const PackedWord &o) const noexcept {
        return word_ != o.word_;
    }

  private:
    uint64_t word_;
};

} // namespace swar
