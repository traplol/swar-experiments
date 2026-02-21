# SWAR Experiments

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## Quick Check

Always run this after making code changes. If it prints FAIL, re-run the full test suite to diagnose.

```bash
./scripts/check.sh
```

## Test

```bash
./build/swar_tests
```

## Benchmark

```bash
./build/swar_bench --benchmark_format=json > results/bench_results.json
```

## Visualize

```bash
source venv/bin/activate
python scripts/visualize.py
```

## Project Layout

- `include/swar/packed_word.hpp` — Core `PackedWord<N>` template. SWAR operations on a single `uint64_t` with N-bit lanes.
- `include/swar/packed_set.hpp` — `PackedSet<N, Capacity>` multi-word set container built on `PackedWord`.
- `include/swar/bucketed_set.hpp` — `BucketedSet<Capacity>` for 11-bit values. Splits by MSB into lo/hi bucket arrays; each `uint64_t` bucket packs 3 × 11-bit lanes with guard bits for SWAR haszero contains.
- `test/packed_word_test.cpp` — gtest suite for all types.
- `bench/packed_word_bench.cpp` — gbench suite, template-instantiated for N=5..14.
- `bench/comparison_bench.cpp` — gbench comparison of PackedSet, BucketedSet, and std containers at N=11.
- `scripts/visualize.py` — Reads gbench JSON from `results/`, produces density and throughput PNGs.
- `scripts/comparison.py` — Runs comparison benchmarks and produces bar chart PNGs.

## Conventions

- Header-only C++17 library. All code lives in `include/swar/`.
- Bit-width `N` is always a compile-time template parameter.
- Register type is `uint64_t` only.
- The haszero trick reserves the MSB of each lane as a guard bit. Operations that depend on this (`contains`, `find`, `count_eq`) require values ≤ `max_safe_value` (= 2^(N-1) - 1). Pure storage ops (`get`, `set`, `broadcast`) allow the full N-bit range.
- `PackedSet` uses 0 as the sentinel for empty lanes, so stored values must be ≥ 1.
- `BucketedSet` is hardcoded to 11-bit values (3×10+1 MSB split). Uses a 2-bit count in bits 34:33 of each `uint64_t` bucket instead of a sentinel. Guard bits at positions 10, 21, 32 enable haszero.
- Tests use googletest v1.14, benchmarks use google/benchmark v1.8.3, both fetched via CMake FetchContent.
- Python venv is in `venv/` (gitignored). Deps in `requirements.txt`: matplotlib, numpy, pandas.
