#include <swar/packed_set.hpp>
#include <swar/packed_word.hpp>

#include <gtest/gtest.h>

using namespace swar;

// ============================================================
// PackedWord compile-time constants
// ============================================================

TEST(PackedWordConstants, LanesFor5Bit) {
    using W = PackedWord<5>;
    EXPECT_EQ(W::bits, 5u);
    EXPECT_EQ(W::lanes, 12u); // 64/5 = 12
    EXPECT_EQ(W::lane_mask, 0x1FULL);
    EXPECT_EQ(W::max_safe_value, 15u); // 2^4 - 1
}

TEST(PackedWordConstants, LanesFor8Bit) {
    using W = PackedWord<8>;
    EXPECT_EQ(W::lanes, 8u);
    EXPECT_EQ(W::lane_mask, 0xFFULL);
    EXPECT_EQ(W::max_safe_value, 127u);
}

TEST(PackedWordConstants, LanesFor14Bit) {
    using W = PackedWord<14>;
    EXPECT_EQ(W::lanes, 4u);
    EXPECT_EQ(W::lane_mask, 0x3FFFULL);
    EXPECT_EQ(W::max_safe_value, 8191u);
}

// ============================================================
// Broadcast
// ============================================================

template <unsigned N> void test_broadcast() {
    using W = PackedWord<N>;
    uint64_t v = 3;
    auto w = W::broadcast(v);
    for (unsigned i = 0; i < W::lanes; ++i) {
        EXPECT_EQ(w.get(i), v) << "N=" << N << " lane=" << i;
    }
}

TEST(PackedWordBroadcast, N5) { test_broadcast<5>(); }
TEST(PackedWordBroadcast, N7) { test_broadcast<7>(); }
TEST(PackedWordBroadcast, N8) { test_broadcast<8>(); }
TEST(PackedWordBroadcast, N10) { test_broadcast<10>(); }
TEST(PackedWordBroadcast, N14) { test_broadcast<14>(); }

// ============================================================
// Get / Set round-trip
// ============================================================

template <unsigned N> void test_get_set_roundtrip() {
    using W = PackedWord<N>;
    W w;
    for (unsigned i = 0; i < W::lanes; ++i) {
        uint64_t val = (i + 1) & W::lane_mask;
        w = w.set(i, val);
    }
    for (unsigned i = 0; i < W::lanes; ++i) {
        uint64_t expected = (i + 1) & W::lane_mask;
        EXPECT_EQ(w.get(i), expected) << "N=" << N << " lane=" << i;
    }
}

TEST(PackedWordGetSet, N5) { test_get_set_roundtrip<5>(); }
TEST(PackedWordGetSet, N6) { test_get_set_roundtrip<6>(); }
TEST(PackedWordGetSet, N8) { test_get_set_roundtrip<8>(); }
TEST(PackedWordGetSet, N11) { test_get_set_roundtrip<11>(); }
TEST(PackedWordGetSet, N14) { test_get_set_roundtrip<14>(); }

// Setting a lane doesn't clobber adjacent lanes
template <unsigned N> void test_set_no_clobber() {
    using W = PackedWord<N>;
    auto w = W::broadcast(1);
    w = w.set(0, W::lane_mask); // max value in lane 0
    EXPECT_EQ(w.get(0), W::lane_mask);
    for (unsigned i = 1; i < W::lanes; ++i) {
        EXPECT_EQ(w.get(i), 1u) << "N=" << N << " lane=" << i;
    }
}

TEST(PackedWordGetSet, NoClobberN5) { test_set_no_clobber<5>(); }
TEST(PackedWordGetSet, NoClobberN14) { test_set_no_clobber<14>(); }

// ============================================================
// Contains
// ============================================================

template <unsigned N> void test_contains() {
    using W = PackedWord<N>;
    W w;
    // Fill lanes with values 1..lanes (all <= max_safe_value for small N)
    for (unsigned i = 0; i < W::lanes; ++i) {
        uint64_t val = (i + 1) % (W::max_safe_value + 1);
        if (val == 0)
            val = 1; // avoid 0
        w = w.set(i, val);
    }
    // Should find value 1
    EXPECT_TRUE(w.contains(1));
    // Should not find a value we didn't insert (if it fits)
    if (W::max_safe_value > W::lanes + 1) {
        EXPECT_FALSE(w.contains(W::max_safe_value));
    }
}

TEST(PackedWordContains, N5) { test_contains<5>(); }
TEST(PackedWordContains, N8) { test_contains<8>(); }
TEST(PackedWordContains, N14) { test_contains<14>(); }

// ============================================================
// Find
// ============================================================

template <unsigned N> void test_find() {
    using W = PackedWord<N>;
    W w;
    for (unsigned i = 0; i < W::lanes; ++i) {
        w = w.set(i, (i + 1));
    }
    EXPECT_EQ(w.find(1), 0);
    EXPECT_EQ(w.find(2), 1);
    // Value not present
    if (W::max_safe_value > W::lanes) {
        EXPECT_EQ(w.find(W::max_safe_value), -1);
    }
}

TEST(PackedWordFind, N5) { test_find<5>(); }
TEST(PackedWordFind, N8) { test_find<8>(); }
TEST(PackedWordFind, N14) { test_find<14>(); }

// ============================================================
// Find zero
// ============================================================

TEST(PackedWordFindZero, EmptyWord) {
    PackedWord<8> w;
    EXPECT_EQ(w.find_zero(), 0);
}

TEST(PackedWordFindZero, FullWord) {
    auto w = PackedWord<8>::broadcast(42);
    EXPECT_EQ(w.find_zero(), -1);
}

TEST(PackedWordFindZero, PartiallyFilled) {
    PackedWord<8> w;
    w = w.set(0, 5);
    w = w.set(1, 10);
    // Lane 2 should be the first zero
    EXPECT_EQ(w.find_zero(), 2);
}

// ============================================================
// Count equal
// ============================================================

TEST(PackedWordCountEq, AllSame) {
    auto w = PackedWord<8>::broadcast(7);
    EXPECT_EQ(w.count_eq(7), PackedWord<8>::lanes);
}

TEST(PackedWordCountEq, NoneMatch) {
    auto w = PackedWord<8>::broadcast(7);
    EXPECT_EQ(w.count_eq(5), 0u);
}

TEST(PackedWordCountEq, SomeMatch) {
    PackedWord<8> w;
    w = w.set(0, 3);
    w = w.set(1, 5);
    w = w.set(2, 3);
    w = w.set(3, 7);
    w = w.set(4, 3);
    EXPECT_EQ(w.count_eq(3), 3u);
    EXPECT_EQ(w.count_eq(5), 1u);
}

// ============================================================
// Min / Max
// ============================================================

TEST(PackedWordMinMax, Simple) {
    PackedWord<8> w;
    w = w.set(0, 10);
    w = w.set(1, 3);
    w = w.set(2, 50);
    w = w.set(3, 7);
    EXPECT_EQ(w.min(4), 3u);
    EXPECT_EQ(w.max(4), 50u);
}

TEST(PackedWordMinMax, AllSame) {
    auto w = PackedWord<10>::broadcast(42);
    EXPECT_EQ(w.min(), 42u);
    EXPECT_EQ(w.max(), 42u);
}

// ============================================================
// PackedSet
// ============================================================

TEST(PackedSet, InsertAndContains) {
    PackedSet<8> s;
    EXPECT_TRUE(s.empty());
    EXPECT_TRUE(s.insert(10));
    EXPECT_TRUE(s.insert(20));
    EXPECT_TRUE(s.insert(30));
    EXPECT_EQ(s.size(), 3u);
    EXPECT_TRUE(s.contains(10));
    EXPECT_TRUE(s.contains(20));
    EXPECT_TRUE(s.contains(30));
    EXPECT_FALSE(s.contains(40));
}

TEST(PackedSet, NoDuplicates) {
    PackedSet<8> s;
    EXPECT_TRUE(s.insert(5));
    EXPECT_FALSE(s.insert(5));
    EXPECT_EQ(s.size(), 1u);
}

TEST(PackedSet, Erase) {
    PackedSet<8> s;
    s.insert(1);
    s.insert(2);
    s.insert(3);
    EXPECT_TRUE(s.erase(2));
    EXPECT_FALSE(s.contains(2));
    EXPECT_EQ(s.size(), 2u);
    EXPECT_FALSE(s.erase(2)); // already gone
}

TEST(PackedSet, SpillsToMultipleWords) {
    PackedSet<8> s;
    // 8-bit lanes: 8 per word. Insert 20 values -> needs 3 words.
    for (uint64_t v = 1; v <= 20; ++v) {
        EXPECT_TRUE(s.insert(v));
    }
    EXPECT_EQ(s.size(), 20u);
    EXPECT_GE(s.word_count(), 3u);
    for (uint64_t v = 1; v <= 20; ++v) {
        EXPECT_TRUE(s.contains(v)) << "missing " << v;
    }
    EXPECT_FALSE(s.contains(21));
}

TEST(PackedSet, SmallBitWidth) {
    // N=5: max_safe_value=15, lanes=12
    PackedSet<5> s;
    for (uint64_t v = 1; v <= 15; ++v) {
        EXPECT_TRUE(s.insert(v));
    }
    EXPECT_EQ(s.size(), 15u);
    for (uint64_t v = 1; v <= 15; ++v) {
        EXPECT_TRUE(s.contains(v));
    }
}
