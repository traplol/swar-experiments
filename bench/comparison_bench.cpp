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

// Number of elements to pre-fill for contains/lookup benchmarks.
// Sweep across sizes to see how each container scales.
static constexpr int64_t kSizes[] = {5, 10, 20, 50, 100, 200, 500};

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

// ============================================================
// INSERT benchmarks — build a set of `size` elements from scratch
// ============================================================

static void BM_Insert_PackedSet(benchmark::State &state) {
    auto sz = static_cast<std::size_t>(state.range(0));
    auto vals = make_values(sz);
    for (auto _ : state) {
        PackedSet<N> s;
        for (auto v : vals)
            s.insert(v);
        benchmark::DoNotOptimize(s);
    }
}

static void BM_Insert_StdSet(benchmark::State &state) {
    auto sz = static_cast<std::size_t>(state.range(0));
    auto vals = make_values(sz);
    for (auto _ : state) {
        std::set<uint16_t> s;
        for (auto v : vals)
            s.insert(v);
        benchmark::DoNotOptimize(s);
    }
}

static void BM_Insert_UnorderedSet(benchmark::State &state) {
    auto sz = static_cast<std::size_t>(state.range(0));
    auto vals = make_values(sz);
    for (auto _ : state) {
        std::unordered_set<uint16_t> s;
        for (auto v : vals)
            s.insert(v);
        benchmark::DoNotOptimize(s);
    }
}

static void BM_Insert_Vector(benchmark::State &state) {
    auto sz = static_cast<std::size_t>(state.range(0));
    auto vals = make_values(sz);
    for (auto _ : state) {
        std::vector<uint16_t> s;
        for (auto v : vals) {
            if (std::find(s.begin(), s.end(), v) == s.end())
                s.push_back(v);
        }
        benchmark::DoNotOptimize(s);
    }
}

// std::array — fixed capacity of 512, linear scan for duplicates
static void BM_Insert_Array(benchmark::State &state) {
    auto sz = static_cast<std::size_t>(state.range(0));
    auto vals = make_values(sz);
    for (auto _ : state) {
        std::array<uint16_t, 512> arr{};
        std::size_t count = 0;
        for (auto v : vals) {
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
    auto sz = static_cast<std::size_t>(state.range(0));
    auto vals = make_values(sz);
    PackedSet<N> s;
    for (auto v : vals)
        s.insert(v);
    uint16_t target = vals[sz / 2];
    for (auto _ : state) {
        bool found = s.contains(target);
        benchmark::DoNotOptimize(found);
    }
}

static void BM_Contains_StdSet(benchmark::State &state) {
    auto sz = static_cast<std::size_t>(state.range(0));
    auto vals = make_values(sz);
    std::set<uint16_t> s(vals.begin(), vals.end());
    uint16_t target = vals[sz / 2];
    for (auto _ : state) {
        bool found = s.count(target) > 0;
        benchmark::DoNotOptimize(found);
    }
}

static void BM_Contains_UnorderedSet(benchmark::State &state) {
    auto sz = static_cast<std::size_t>(state.range(0));
    auto vals = make_values(sz);
    std::unordered_set<uint16_t> s(vals.begin(), vals.end());
    uint16_t target = vals[sz / 2];
    for (auto _ : state) {
        bool found = s.count(target) > 0;
        benchmark::DoNotOptimize(found);
    }
}

static void BM_Contains_Vector(benchmark::State &state) {
    auto sz = static_cast<std::size_t>(state.range(0));
    auto vals = make_values(sz);
    std::vector<uint16_t> s(vals.begin(), vals.end());
    uint16_t target = vals[sz / 2];
    for (auto _ : state) {
        bool found = std::find(s.begin(), s.end(), target) != s.end();
        benchmark::DoNotOptimize(found);
    }
}

static void BM_Contains_Array(benchmark::State &state) {
    auto sz = static_cast<std::size_t>(state.range(0));
    auto vals = make_values(sz);
    std::array<uint16_t, 512> arr{};
    for (std::size_t i = 0; i < sz; ++i)
        arr[i] = vals[i];
    uint16_t target = vals[sz / 2];
    for (auto _ : state) {
        bool found = false;
        for (std::size_t i = 0; i < sz; ++i) {
            if (arr[i] == target) { found = true; break; }
        }
        benchmark::DoNotOptimize(found);
    }
}

// ============================================================
// CONTAINS (miss) benchmarks — lookup a value NOT in the set
// ============================================================

static void BM_ContainsMiss_PackedSet(benchmark::State &state) {
    auto sz = static_cast<std::size_t>(state.range(0));
    auto vals = make_values(sz, 99);
    PackedSet<N> s;
    for (auto v : vals)
        s.insert(v);
    // Pick a value guaranteed not present (use seed collision avoidance)
    uint16_t target = PW::max_safe_value; // very unlikely in a small set
    s.erase(target);                      // ensure it's absent
    for (auto _ : state) {
        bool found = s.contains(target);
        benchmark::DoNotOptimize(found);
    }
}

static void BM_ContainsMiss_StdSet(benchmark::State &state) {
    auto sz = static_cast<std::size_t>(state.range(0));
    auto vals = make_values(sz, 99);
    std::set<uint16_t> s(vals.begin(), vals.end());
    s.erase(PW::max_safe_value);
    for (auto _ : state) {
        bool found = s.count(PW::max_safe_value) > 0;
        benchmark::DoNotOptimize(found);
    }
}

static void BM_ContainsMiss_UnorderedSet(benchmark::State &state) {
    auto sz = static_cast<std::size_t>(state.range(0));
    auto vals = make_values(sz, 99);
    std::unordered_set<uint16_t> s(vals.begin(), vals.end());
    s.erase(PW::max_safe_value);
    for (auto _ : state) {
        bool found = s.count(PW::max_safe_value) > 0;
        benchmark::DoNotOptimize(found);
    }
}

static void BM_ContainsMiss_Vector(benchmark::State &state) {
    auto sz = static_cast<std::size_t>(state.range(0));
    auto vals = make_values(sz, 99);
    std::vector<uint16_t> s(vals.begin(), vals.end());
    auto it = std::find(s.begin(), s.end(), PW::max_safe_value);
    if (it != s.end()) s.erase(it);
    for (auto _ : state) {
        bool found = std::find(s.begin(), s.end(),
                               static_cast<uint16_t>(PW::max_safe_value)) != s.end();
        benchmark::DoNotOptimize(found);
    }
}

static void BM_ContainsMiss_Array(benchmark::State &state) {
    auto sz = static_cast<std::size_t>(state.range(0));
    auto vals = make_values(sz, 99);
    std::array<uint16_t, 512> arr{};
    std::size_t count = 0;
    for (std::size_t i = 0; i < sz; ++i) {
        if (vals[i] != PW::max_safe_value)
            arr[count++] = vals[i];
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
// Register all benchmarks with size sweeps
// ============================================================

#define APPLY_SIZES(fn)                                                        \
    BENCHMARK(fn)->Arg(5)->Arg(10)->Arg(20)->Arg(50)->Arg(100)->Arg(200)->Arg(500)

APPLY_SIZES(BM_Insert_PackedSet);
APPLY_SIZES(BM_Insert_StdSet);
APPLY_SIZES(BM_Insert_UnorderedSet);
APPLY_SIZES(BM_Insert_Vector);
APPLY_SIZES(BM_Insert_Array);

APPLY_SIZES(BM_Contains_PackedSet);
APPLY_SIZES(BM_Contains_StdSet);
APPLY_SIZES(BM_Contains_UnorderedSet);
APPLY_SIZES(BM_Contains_Vector);
APPLY_SIZES(BM_Contains_Array);

APPLY_SIZES(BM_ContainsMiss_PackedSet);
APPLY_SIZES(BM_ContainsMiss_StdSet);
APPLY_SIZES(BM_ContainsMiss_UnorderedSet);
APPLY_SIZES(BM_ContainsMiss_Vector);
APPLY_SIZES(BM_ContainsMiss_Array);
