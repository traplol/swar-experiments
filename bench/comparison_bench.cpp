#include <swar/packed_set.hpp>
#include <swar/packed_word.hpp>

#include <algorithm>
#include <array>
#include <benchmark/benchmark.h>
#include <random>
#include <set>
#include <unordered_set>
#include <vector>

using namespace swar;

// All comparisons use N=11: max_safe_value=1023, 5 lanes per word.
static constexpr unsigned N = 11;
using PW = PackedWord<N>;

// Fixed set size for all benchmarks.
static constexpr std::size_t kSize = 10;

// ---------- Helpers ----------

static std::vector<uint16_t> make_values(std::size_t count, uint64_t seed = 42) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<uint16_t> dist(1, PW::max_safe_value);
    std::unordered_set<uint16_t> seen;
    std::vector<uint16_t> out;
    out.reserve(count);
    while (out.size() < count) {
        auto v = dist(rng);
        if (seen.insert(v).second)
            out.push_back(v);
    }
    return out;
}

static const auto kVals = make_values(kSize);
static const auto kValsMiss = make_values(kSize, 99);

// ============================================================
// INSERT benchmarks — build a set of 10 elements from scratch
// ============================================================

static void BM_Insert_PackedSet(benchmark::State &state) {
    for (auto _ : state) {
        PackedSet<N> s;
        for (auto v : kVals)
            s.insert(v);
        benchmark::DoNotOptimize(s);
    }
}

static void BM_Insert_StdSet(benchmark::State &state) {
    for (auto _ : state) {
        std::set<uint16_t> s;
        for (auto v : kVals)
            s.insert(v);
        benchmark::DoNotOptimize(s);
    }
}

static void BM_Insert_UnorderedSet(benchmark::State &state) {
    for (auto _ : state) {
        std::unordered_set<uint16_t> s;
        for (auto v : kVals)
            s.insert(v);
        benchmark::DoNotOptimize(s);
    }
}

static void BM_Insert_Vector(benchmark::State &state) {
    for (auto _ : state) {
        std::vector<uint16_t> s;
        for (auto v : kVals) {
            if (std::find(s.begin(), s.end(), v) == s.end())
                s.push_back(v);
        }
        benchmark::DoNotOptimize(s);
    }
}

static void BM_Insert_Array(benchmark::State &state) {
    for (auto _ : state) {
        std::array<uint16_t, kSize> arr{};
        std::size_t count = 0;
        for (auto v : kVals) {
            bool found = false;
            for (std::size_t i = 0; i < count; ++i) {
                if (arr[i] == v) { found = true; break; }
            }
            if (!found)
                arr[count++] = v;
        }
        benchmark::DoNotOptimize(arr);
        benchmark::DoNotOptimize(count);
    }
}

// ============================================================
// CONTAINS (hit) benchmarks — lookup a known-present value
// ============================================================

static void BM_Contains_PackedSet(benchmark::State &state) {
    PackedSet<N> s;
    for (auto v : kVals)
        s.insert(v);
    uint16_t target = kVals[kSize / 2];
    for (auto _ : state) {
        bool found = s.contains(target);
        benchmark::DoNotOptimize(found);
    }
}

static void BM_Contains_StdSet(benchmark::State &state) {
    std::set<uint16_t> s(kVals.begin(), kVals.end());
    uint16_t target = kVals[kSize / 2];
    for (auto _ : state) {
        bool found = s.count(target) > 0;
        benchmark::DoNotOptimize(found);
    }
}

static void BM_Contains_UnorderedSet(benchmark::State &state) {
    std::unordered_set<uint16_t> s(kVals.begin(), kVals.end());
    uint16_t target = kVals[kSize / 2];
    for (auto _ : state) {
        bool found = s.count(target) > 0;
        benchmark::DoNotOptimize(found);
    }
}

static void BM_Contains_Vector(benchmark::State &state) {
    std::vector<uint16_t> s(kVals.begin(), kVals.end());
    uint16_t target = kVals[kSize / 2];
    for (auto _ : state) {
        bool found = std::find(s.begin(), s.end(), target) != s.end();
        benchmark::DoNotOptimize(found);
    }
}

static void BM_Contains_Array(benchmark::State &state) {
    std::array<uint16_t, kSize> arr{};
    for (std::size_t i = 0; i < kSize; ++i)
        arr[i] = kVals[i];
    uint16_t target = kVals[kSize / 2];
    for (auto _ : state) {
        bool found = false;
        for (std::size_t i = 0; i < kSize; ++i) {
            if (arr[i] == target) { found = true; break; }
        }
        benchmark::DoNotOptimize(found);
    }
}

// ============================================================
// CONTAINS (miss) benchmarks — lookup a value NOT in the set
// ============================================================

static void BM_ContainsMiss_PackedSet(benchmark::State &state) {
    PackedSet<N> s;
    for (auto v : kValsMiss)
        s.insert(v);
    uint16_t target = PW::max_safe_value;
    s.erase(target); // ensure it's absent
    for (auto _ : state) {
        bool found = s.contains(target);
        benchmark::DoNotOptimize(found);
    }
}

static void BM_ContainsMiss_StdSet(benchmark::State &state) {
    std::set<uint16_t> s(kValsMiss.begin(), kValsMiss.end());
    s.erase(PW::max_safe_value);
    for (auto _ : state) {
        bool found = s.count(PW::max_safe_value) > 0;
        benchmark::DoNotOptimize(found);
    }
}

static void BM_ContainsMiss_UnorderedSet(benchmark::State &state) {
    std::unordered_set<uint16_t> s(kValsMiss.begin(), kValsMiss.end());
    s.erase(PW::max_safe_value);
    for (auto _ : state) {
        bool found = s.count(PW::max_safe_value) > 0;
        benchmark::DoNotOptimize(found);
    }
}

static void BM_ContainsMiss_Vector(benchmark::State &state) {
    std::vector<uint16_t> s(kValsMiss.begin(), kValsMiss.end());
    auto it = std::find(s.begin(), s.end(), static_cast<uint16_t>(PW::max_safe_value));
    if (it != s.end()) s.erase(it);
    for (auto _ : state) {
        bool found = std::find(s.begin(), s.end(),
                               static_cast<uint16_t>(PW::max_safe_value)) != s.end();
        benchmark::DoNotOptimize(found);
    }
}

static void BM_ContainsMiss_Array(benchmark::State &state) {
    std::array<uint16_t, kSize> arr{};
    std::size_t count = 0;
    for (std::size_t i = 0; i < kSize; ++i) {
        if (kValsMiss[i] != PW::max_safe_value)
            arr[count++] = kValsMiss[i];
    }
    auto needle = static_cast<uint16_t>(PW::max_safe_value);
    for (auto _ : state) {
        bool found = false;
        for (std::size_t i = 0; i < count; ++i) {
            if (arr[i] == needle) { found = true; break; }
        }
        benchmark::DoNotOptimize(found);
    }
}

// ============================================================
// Register all benchmarks
// ============================================================

BENCHMARK(BM_Insert_PackedSet);
BENCHMARK(BM_Insert_StdSet);
BENCHMARK(BM_Insert_UnorderedSet);
BENCHMARK(BM_Insert_Vector);
BENCHMARK(BM_Insert_Array);

BENCHMARK(BM_Contains_PackedSet);
BENCHMARK(BM_Contains_StdSet);
BENCHMARK(BM_Contains_UnorderedSet);
BENCHMARK(BM_Contains_Vector);
BENCHMARK(BM_Contains_Array);

BENCHMARK(BM_ContainsMiss_PackedSet);
BENCHMARK(BM_ContainsMiss_StdSet);
BENCHMARK(BM_ContainsMiss_UnorderedSet);
BENCHMARK(BM_ContainsMiss_Vector);
BENCHMARK(BM_ContainsMiss_Array);
