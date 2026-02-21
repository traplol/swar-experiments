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
//static constexpr std::size_t kSize = 10;
static constexpr std::size_t kSize = 5;

// Store N and kSize in benchmark counters so visualization can read them.
#define SET_COUNTERS(state)                                                    \
    state.counters["N"] = N;                                                   \
    state.counters["size"] = kSize;

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
    SET_COUNTERS(state);
    for (auto _ : state) {
        PackedSet<N, kSize> s;
        for (auto v : kVals)
            s.insert(v);
        benchmark::DoNotOptimize(s);
    }
}

static void BM_Insert_StdSet(benchmark::State &state) {
    SET_COUNTERS(state);
    for (auto _ : state) {
        std::set<uint16_t> s;
        for (auto v : kVals)
            s.insert(v);
        benchmark::DoNotOptimize(s);
    }
}

static void BM_Insert_UnorderedSet(benchmark::State &state) {
    SET_COUNTERS(state);
    for (auto _ : state) {
        std::unordered_set<uint16_t> s;
        for (auto v : kVals)
            s.insert(v);
        benchmark::DoNotOptimize(s);
    }
}

static void BM_Insert_Vector(benchmark::State &state) {
    SET_COUNTERS(state);
    for (auto _ : state) {
        std::vector<uint16_t> s;
        for (auto v : kVals) {
            if (std::find(s.begin(), s.end(), v) == s.end())
                s.push_back(v);
        }
        benchmark::DoNotOptimize(s);
    }
}

static void BM_Insert_SortedVector(benchmark::State &state) {
    SET_COUNTERS(state);
    for (auto _ : state) {
        std::vector<uint16_t> s;
        for (auto v : kVals) {
            auto it = std::lower_bound(s.begin(), s.end(), v);
            if (it == s.end() || *it != v)
                s.insert(it, v);
        }
        benchmark::DoNotOptimize(s);
    }
}

static void BM_Insert_Array(benchmark::State &state) {
    SET_COUNTERS(state);
    for (auto _ : state) {
        std::array<uint16_t, kSize> arr{};
        std::size_t count = 0;
        benchmark::ClobberMemory();
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
    SET_COUNTERS(state);
    PackedSet<N, kSize> s;
    for (auto v : kVals)
        s.insert(v);
    uint16_t target = kVals[kSize / 2];
    for (auto _ : state) {
        bool found = s.contains(target);
        benchmark::DoNotOptimize(found);
    }
}

static void BM_Contains_StdSet(benchmark::State &state) {
    SET_COUNTERS(state);
    std::set<uint16_t> s(kVals.begin(), kVals.end());
    uint16_t target = kVals[kSize / 2];
    for (auto _ : state) {
        bool found = s.count(target) > 0;
        benchmark::DoNotOptimize(found);
    }
}

static void BM_Contains_UnorderedSet(benchmark::State &state) {
    SET_COUNTERS(state);
    std::unordered_set<uint16_t> s(kVals.begin(), kVals.end());
    uint16_t target = kVals[kSize / 2];
    for (auto _ : state) {
        bool found = s.count(target) > 0;
        benchmark::DoNotOptimize(found);
    }
}

static void BM_Contains_Vector(benchmark::State &state) {
    SET_COUNTERS(state);
    std::vector<uint16_t> s(kVals.begin(), kVals.end());
    uint16_t target = kVals[kSize / 2];
    for (auto _ : state) {
        bool found = std::find(s.begin(), s.end(), target) != s.end();
        benchmark::DoNotOptimize(found);
    }
}

static void BM_Contains_SortedVector(benchmark::State &state) {
    SET_COUNTERS(state);
    std::vector<uint16_t> s(kVals.begin(), kVals.end());
    std::sort(s.begin(), s.end());
    uint16_t target = kVals[kSize / 2];
    for (auto _ : state) {
        bool found = std::binary_search(s.begin(), s.end(), target);
        benchmark::DoNotOptimize(found);
    }
}

static void BM_Contains_Array(benchmark::State &state) {
    SET_COUNTERS(state);
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
    SET_COUNTERS(state);
    PackedSet<N, kSize> s;
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
    SET_COUNTERS(state);
    std::set<uint16_t> s(kValsMiss.begin(), kValsMiss.end());
    s.erase(PW::max_safe_value);
    for (auto _ : state) {
        bool found = s.count(PW::max_safe_value) > 0;
        benchmark::DoNotOptimize(found);
    }
}

static void BM_ContainsMiss_UnorderedSet(benchmark::State &state) {
    SET_COUNTERS(state);
    std::unordered_set<uint16_t> s(kValsMiss.begin(), kValsMiss.end());
    s.erase(PW::max_safe_value);
    for (auto _ : state) {
        bool found = s.count(PW::max_safe_value) > 0;
        benchmark::DoNotOptimize(found);
    }
}

static void BM_ContainsMiss_Vector(benchmark::State &state) {
    SET_COUNTERS(state);
    std::vector<uint16_t> s(kValsMiss.begin(), kValsMiss.end());
    auto it = std::find(s.begin(), s.end(), static_cast<uint16_t>(PW::max_safe_value));
    if (it != s.end()) s.erase(it);
    for (auto _ : state) {
        bool found = std::find(s.begin(), s.end(),
                               static_cast<uint16_t>(PW::max_safe_value)) != s.end();
        benchmark::DoNotOptimize(found);
    }
}

static void BM_ContainsMiss_SortedVector(benchmark::State &state) {
    SET_COUNTERS(state);
    std::vector<uint16_t> s(kValsMiss.begin(), kValsMiss.end());
    std::sort(s.begin(), s.end());
    s.erase(std::remove(s.begin(), s.end(), static_cast<uint16_t>(PW::max_safe_value)), s.end());
    for (auto _ : state) {
        bool found = std::binary_search(s.begin(), s.end(),
                                        static_cast<uint16_t>(PW::max_safe_value));
        benchmark::DoNotOptimize(found);
    }
}

static void BM_ContainsMiss_Array(benchmark::State &state) {
    SET_COUNTERS(state);
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
// ERASE benchmarks — erase a known-present value from a full set
// Each iteration rebuilds the set so we always erase from a full one.
// ============================================================

static void BM_Erase_PackedSet(benchmark::State &state) {
    SET_COUNTERS(state);
    uint16_t target = kVals[kSize / 2];
    PackedSet<N, kSize> base;
    for (auto v : kVals) base.insert(v);
    for (auto _ : state) {
        auto s = base;
        bool ok = s.erase(target);
        benchmark::DoNotOptimize(ok);
        benchmark::DoNotOptimize(s);
    }
}

static void BM_Erase_StdSet(benchmark::State &state) {
    SET_COUNTERS(state);
    uint16_t target = kVals[kSize / 2];
    std::set<uint16_t> base(kVals.begin(), kVals.end());
    for (auto _ : state) {
        auto s = base;
        auto n = s.erase(target);
        benchmark::DoNotOptimize(n);
        benchmark::DoNotOptimize(s);
    }
}

static void BM_Erase_UnorderedSet(benchmark::State &state) {
    SET_COUNTERS(state);
    uint16_t target = kVals[kSize / 2];
    std::unordered_set<uint16_t> base(kVals.begin(), kVals.end());
    for (auto _ : state) {
        auto s = base;
        auto n = s.erase(target);
        benchmark::DoNotOptimize(n);
        benchmark::DoNotOptimize(s);
    }
}

static void BM_Erase_Vector(benchmark::State &state) {
    SET_COUNTERS(state);
    uint16_t target = kVals[kSize / 2];
    std::vector<uint16_t> base(kVals.begin(), kVals.end());
    for (auto _ : state) {
        auto s = base;
        auto it = std::find(s.begin(), s.end(), target);
        if (it != s.end()) s.erase(it);
        benchmark::DoNotOptimize(s);
    }
}

static void BM_Erase_SortedVector(benchmark::State &state) {
    SET_COUNTERS(state);
    uint16_t target = kVals[kSize / 2];
    std::vector<uint16_t> base(kVals.begin(), kVals.end());
    std::sort(base.begin(), base.end());
    for (auto _ : state) {
        auto s = base;
        auto it = std::lower_bound(s.begin(), s.end(), target);
        if (it != s.end() && *it == target) s.erase(it);
        benchmark::DoNotOptimize(s);
    }
}

static void BM_Erase_Array(benchmark::State &state) {
    SET_COUNTERS(state);
    uint16_t target = kVals[kSize / 2];
    std::array<uint16_t, kSize> base{};
    for (std::size_t i = 0; i < kSize; ++i) base[i] = kVals[i];
    for (auto _ : state) {
        auto arr = base;
        std::size_t count = kSize;
        benchmark::DoNotOptimize(arr.data());
        benchmark::ClobberMemory();
        for (std::size_t i = 0; i < count; ++i) {
            if (arr[i] == target) {
                arr[i] = arr[--count]; // swap-remove
                break;
            }
        }
        benchmark::DoNotOptimize(arr.data());
        benchmark::ClobberMemory();
    }
}

// ============================================================
// MEMORY benchmarks — measure bytes used by each container
// Uses a tracking allocator for heap containers.
// ============================================================

static thread_local std::size_t g_alloc_bytes = 0;

template <typename T>
struct TrackingAllocator {
    using value_type = T;
    TrackingAllocator() = default;
    template <typename U>
    TrackingAllocator(const TrackingAllocator<U> &) noexcept {}
    T *allocate(std::size_t n) {
        g_alloc_bytes += n * sizeof(T);
        return std::allocator<T>{}.allocate(n);
    }
    void deallocate(T *p, std::size_t n) noexcept {
        std::allocator<T>{}.deallocate(p, n);
    }
    template <typename U>
    bool operator==(const TrackingAllocator<U> &) const noexcept { return true; }
    template <typename U>
    bool operator!=(const TrackingAllocator<U> &) const noexcept { return false; }
};

static void BM_Memory_PackedSet(benchmark::State &state) {
    SET_COUNTERS(state);
    using PS = PackedSet<N, kSize>;
    state.counters["bytes"] = sizeof(PS);
    for (auto _ : state) {
        PS s;
        for (auto v : kVals) s.insert(v);
        benchmark::DoNotOptimize(s);
    }
}

static void BM_Memory_StdSet(benchmark::State &state) {
    SET_COUNTERS(state);
    for (auto _ : state) {
        g_alloc_bytes = 0;
        std::set<uint16_t, std::less<uint16_t>, TrackingAllocator<uint16_t>> s;
        for (auto v : kVals) s.insert(v);
        benchmark::DoNotOptimize(s);
        state.counters["bytes"] = sizeof(s) + g_alloc_bytes;
    }
}

static void BM_Memory_UnorderedSet(benchmark::State &state) {
    SET_COUNTERS(state);
    for (auto _ : state) {
        g_alloc_bytes = 0;
        std::unordered_set<uint16_t, std::hash<uint16_t>, std::equal_to<uint16_t>,
                           TrackingAllocator<uint16_t>> s;
        for (auto v : kVals) s.insert(v);
        benchmark::DoNotOptimize(s);
        state.counters["bytes"] = sizeof(s) + g_alloc_bytes;
    }
}

static void BM_Memory_Vector(benchmark::State &state) {
    SET_COUNTERS(state);
    for (auto _ : state) {
        g_alloc_bytes = 0;
        std::vector<uint16_t, TrackingAllocator<uint16_t>> s;
        for (auto v : kVals) {
            if (std::find(s.begin(), s.end(), v) == s.end())
                s.push_back(v);
        }
        benchmark::DoNotOptimize(s);
        state.counters["bytes"] = sizeof(s) + g_alloc_bytes;
    }
}

static void BM_Memory_SortedVector(benchmark::State &state) {
    SET_COUNTERS(state);
    for (auto _ : state) {
        g_alloc_bytes = 0;
        std::vector<uint16_t, TrackingAllocator<uint16_t>> s;
        for (auto v : kVals) {
            auto it = std::lower_bound(s.begin(), s.end(), v);
            if (it == s.end() || *it != v)
                s.insert(it, v);
        }
        benchmark::DoNotOptimize(s);
        state.counters["bytes"] = sizeof(s) + g_alloc_bytes;
    }
}

static void BM_Memory_Array(benchmark::State &state) {
    SET_COUNTERS(state);
    state.counters["bytes"] = sizeof(std::array<uint16_t, kSize>);
    for (auto _ : state) {
        std::array<uint16_t, kSize> arr{};
        std::size_t count = 0;
        for (auto v : kVals) {
            bool found = false;
            for (std::size_t i = 0; i < count; ++i) {
                if (arr[i] == v) { found = true; break; }
            }
            if (!found) arr[count++] = v;
        }
        benchmark::DoNotOptimize(arr);
    }
}

// ============================================================
// Register all benchmarks
// ============================================================

BENCHMARK(BM_Insert_PackedSet);
BENCHMARK(BM_Insert_StdSet);
BENCHMARK(BM_Insert_UnorderedSet);
BENCHMARK(BM_Insert_Vector);
BENCHMARK(BM_Insert_SortedVector);
BENCHMARK(BM_Insert_Array);

BENCHMARK(BM_Contains_PackedSet);
BENCHMARK(BM_Contains_StdSet);
BENCHMARK(BM_Contains_UnorderedSet);
BENCHMARK(BM_Contains_Vector);
BENCHMARK(BM_Contains_SortedVector);
BENCHMARK(BM_Contains_Array);

BENCHMARK(BM_ContainsMiss_PackedSet);
BENCHMARK(BM_ContainsMiss_StdSet);
BENCHMARK(BM_ContainsMiss_UnorderedSet);
BENCHMARK(BM_ContainsMiss_Vector);
BENCHMARK(BM_ContainsMiss_SortedVector);
BENCHMARK(BM_ContainsMiss_Array);

BENCHMARK(BM_Erase_PackedSet);
BENCHMARK(BM_Erase_StdSet);
BENCHMARK(BM_Erase_UnorderedSet);
BENCHMARK(BM_Erase_Vector);
BENCHMARK(BM_Erase_SortedVector);
BENCHMARK(BM_Erase_Array);

BENCHMARK(BM_Memory_PackedSet);
BENCHMARK(BM_Memory_StdSet);
BENCHMARK(BM_Memory_UnorderedSet);
BENCHMARK(BM_Memory_Vector);
BENCHMARK(BM_Memory_SortedVector);
BENCHMARK(BM_Memory_Array);
