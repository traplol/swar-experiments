#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace swar {

/// A fixed-capacity set of 11-bit integers, stored using MSB-shared bucket
/// packing. Each uint64_t bucket holds 3 × 11-bit lanes (10 data + 1 guard)
/// with a 2-bit lane count in bits 34:33.
///
/// Bucket layout (uint64_t):
///   [...unused...][count:2][G2][val2:10][G1][val1:10][G0][val0:10]
///      63..35       34:33   32  31..22   21  20..11   10   9..0
///
/// Guard bits (G0=bit10, G1=bit21, G2=bit32) enable the haszero SWAR trick
/// for parallel 3-lane matching in a single operation.
///
/// count (bits 34:33): number of occupied lanes (0-3).
///
/// Values are split by MSB (bit 10): lo_buckets_ stores values [1,1023],
/// hi_buckets_ stores values [1024,2047]. The lower 10 bits are stored
/// in the lane data bits; guard bits are always 0 for valid data.
template <std::size_t Capacity>
class BucketedSet {
    static_assert(Capacity > 0, "Capacity must be > 0");

  public:
    static constexpr unsigned value_bits = 11;
    static constexpr uint16_t max_value = (1u << value_bits) - 1; // 2047
    static constexpr unsigned lanes_per_bucket = 3;
    static constexpr std::size_t buckets_per_half =
        (Capacity + lanes_per_bucket - 1) / lanes_per_bucket;
    static constexpr std::size_t capacity = Capacity;

    constexpr BucketedSet() noexcept : lo_buckets_{}, hi_buckets_{} {}

    bool insert(uint16_t v) {
        assert(v >= 1 && v <= max_value);
        uint32_t msb = v >> 10;
        uint16_t lo = v & 0x3FF;
        auto &buckets = msb ? hi_buckets_ : lo_buckets_;

        // Check for duplicate
        for (const auto &b : buckets) {
            if (bucket_contains(b, lo))
                return false;
        }

        // Find a bucket with a free lane
        for (auto &b : buckets) {
            unsigned cnt = bucket_count(b);
            if (cnt < lanes_per_bucket) {
                b = set_count(bucket_set(b, cnt, lo), cnt + 1);
                return true;
            }
        }
        return false; // full
    }

    bool erase(uint16_t v) {
        assert(v >= 1 && v <= max_value);
        uint32_t msb = v >> 10;
        uint16_t lo = v & 0x3FF;
        auto &buckets = msb ? hi_buckets_ : lo_buckets_;

        for (auto &b : buckets) {
            int lane = bucket_find(b, lo);
            if (lane >= 0) {
                unsigned cnt = bucket_count(b);
                uint16_t last = bucket_get(b, cnt - 1);
                b = bucket_set(b, static_cast<unsigned>(lane), last);
                b = bucket_set(b, cnt - 1, 0);
                b = set_count(b, cnt - 1);
                return true;
            }
        }
        return false;
    }

    bool contains(uint16_t v) const {
        assert(v >= 1 && v <= max_value);
        uint32_t msb = v >> 10;
        uint16_t lo = v & 0x3FF;
        const auto &buckets = msb ? hi_buckets_ : lo_buckets_;

        for (const auto &b : buckets) {
            if (bucket_contains(b, lo))
                return true;
        }
        return false;
    }

    static constexpr std::size_t size() noexcept { return capacity; }

  private:
    // 11-bit lane constants for 3-lane SWAR in uint64_t
    static constexpr unsigned lane_bits = 11;
    static constexpr uint64_t lane_mask = 0x3FFu; // 10 data bits per lane
    static constexpr unsigned count_shift = 33;

    // haszero constants: broadcast_one has a 1 at the LSB of each 11-bit lane
    static constexpr uint64_t broadcast_one =
        (1ULL) | (1ULL << 11) | (1ULL << 22);
    // high_bits has the guard bit (MSB) of each 11-bit lane set
    static constexpr uint64_t high_bits =
        (1ULL << 10) | (1ULL << 21) | (1ULL << 32);
    // Mask for all 3 lanes' data+guard bits (33 bits)
    static constexpr uint64_t all_lanes = (1ULL << 33) - 1;

    // Count-based validity masks: after haszero, result bits appear at
    // guard-bit positions (10, 21, 32). Mask by count to ignore empty lanes.
    static constexpr uint64_t count_masks[4] = {
        0,
        (1ULL << 10),
        (1ULL << 10) | (1ULL << 21),
        (1ULL << 10) | (1ULL << 21) | (1ULL << 32),
    };

    /// SWAR haszero contains: XOR with broadcast, detect zero lanes.
    static constexpr bool bucket_contains(uint64_t b, uint16_t lo) {
        uint64_t data = b & all_lanes;
        uint64_t bcast = static_cast<uint64_t>(lo) * broadcast_one;
        uint64_t xored = data ^ bcast;
        uint64_t hz = (xored - broadcast_one) & ~xored & high_bits;
        return (hz & count_masks[b >> count_shift]) != 0;
    }

    /// SWAR find: returns lane index of match, or -1.
    static constexpr int bucket_find(uint64_t b, uint16_t lo) {
        uint64_t data = b & all_lanes;
        uint64_t bcast = static_cast<uint64_t>(lo) * broadcast_one;
        uint64_t xored = data ^ bcast;
        uint64_t hz = (xored - broadcast_one) & ~xored & high_bits;
        hz &= count_masks[b >> count_shift];
        if (hz == 0) return -1;
        return static_cast<int>(__builtin_ctzll(hz) / lane_bits);
    }

    static constexpr unsigned bucket_count(uint64_t b) {
        return static_cast<unsigned>(b >> count_shift);
    }

    static constexpr uint64_t set_count(uint64_t b, unsigned cnt) {
        return (b & ~(3ULL << count_shift))
             | (static_cast<uint64_t>(cnt) << count_shift);
    }

    static constexpr uint16_t bucket_get(uint64_t b, unsigned lane) {
        return static_cast<uint16_t>((b >> (lane * lane_bits)) & lane_mask);
    }

    static constexpr uint64_t bucket_set(uint64_t b, unsigned lane,
                                         uint16_t val10) {
        unsigned shift = lane * lane_bits;
        uint64_t mask = lane_mask << shift;
        return (b & ~mask) | (static_cast<uint64_t>(val10) << shift);
    }

    std::array<uint64_t, buckets_per_half> lo_buckets_;
    std::array<uint64_t, buckets_per_half> hi_buckets_;
};

} // namespace swar
