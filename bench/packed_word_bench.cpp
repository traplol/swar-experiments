#include <swar/packed_set.hpp>
#include <swar/packed_word.hpp>

#include <benchmark/benchmark.h>
#include <random>

using namespace swar;

// ---------- Helpers ----------

// Pre-fill a word with random valid values
template <unsigned N>
PackedWord<N> make_full_word(std::mt19937_64 &rng) {
    using W = PackedWord<N>;
    std::uniform_int_distribution<uint64_t> dist(1, W::max_safe_value);
    W w;
    for (unsigned i = 0; i < W::lanes; ++i)
        w = w.set(i, dist(rng));
    return w;
}

// ---------- Broadcast ----------

template <unsigned N> static void BM_Broadcast(benchmark::State &state) {
    using W = PackedWord<N>;
    uint64_t v = 7;
    for (auto _ : state) {
        auto w = W::broadcast(v);
        benchmark::DoNotOptimize(w);
    }
}

// ---------- Extract (get) ----------

template <unsigned N> static void BM_Extract(benchmark::State &state) {
    using W = PackedWord<N>;
    std::mt19937_64 rng(42);
    auto w = make_full_word<N>(rng);
    unsigned lane = 0;
    for (auto _ : state) {
        auto v = w.get(lane);
        benchmark::DoNotOptimize(v);
        lane = (lane + 1) % W::lanes;
    }
}

// ---------- Contains (hit) ----------

template <unsigned N> static void BM_ContainsHit(benchmark::State &state) {
    using W = PackedWord<N>;
    std::mt19937_64 rng(42);
    auto w = make_full_word<N>(rng);
    uint64_t target = w.get(W::lanes / 2); // pick a value we know is there
    for (auto _ : state) {
        bool found = w.contains(target);
        benchmark::DoNotOptimize(found);
    }
}

// ---------- Contains (miss) ----------

template <unsigned N> static void BM_ContainsMiss(benchmark::State &state) {
    using W = PackedWord<N>;
    // Fill with value 1, search for max_safe_value
    auto w = W::broadcast(1);
    for (auto _ : state) {
        bool found = w.contains(W::max_safe_value);
        benchmark::DoNotOptimize(found);
    }
}

// ---------- Find ----------

template <unsigned N> static void BM_Find(benchmark::State &state) {
    using W = PackedWord<N>;
    std::mt19937_64 rng(42);
    auto w = make_full_word<N>(rng);
    uint64_t target = w.get(W::lanes - 1); // last lane
    for (auto _ : state) {
        int idx = w.find(target);
        benchmark::DoNotOptimize(idx);
    }
}

// ---------- PackedSet insert ----------

template <unsigned N> static void BM_SetInsert(benchmark::State &state) {
    using W = PackedWord<N>;
    for (auto _ : state) {
        PackedSet<N, 64> s;
        for (uint64_t v = 1;
             v <= W::max_safe_value && v <= 64; ++v) {
            s.insert(v);
        }
        benchmark::DoNotOptimize(s);
    }
}

// ---------- PackedSet contains ----------

template <unsigned N> static void BM_SetContains(benchmark::State &state) {
    using W = PackedWord<N>;
    PackedSet<N, 64> s;
    uint64_t cap = std::min<uint64_t>(W::max_safe_value, 64);
    for (uint64_t v = 1; v <= cap; ++v)
        s.insert(v);
    uint64_t target = cap / 2;
    for (auto _ : state) {
        bool found = s.contains(target);
        benchmark::DoNotOptimize(found);
    }
}

// ---------- Register benchmarks for N = 5..14 ----------

#define REGISTER_ALL(N)                                                        \
    BENCHMARK(BM_Broadcast<N>);                                                \
    BENCHMARK(BM_Extract<N>);                                                  \
    BENCHMARK(BM_ContainsHit<N>);                                              \
    BENCHMARK(BM_ContainsMiss<N>);                                             \
    BENCHMARK(BM_Find<N>);                                                     \
    BENCHMARK(BM_SetInsert<N>);                                                \
    BENCHMARK(BM_SetContains<N>);

REGISTER_ALL(5)
REGISTER_ALL(6)
REGISTER_ALL(7)
REGISTER_ALL(8)
REGISTER_ALL(9)
REGISTER_ALL(10)
REGISTER_ALL(11)
REGISTER_ALL(12)
REGISTER_ALL(13)
REGISTER_ALL(14)
