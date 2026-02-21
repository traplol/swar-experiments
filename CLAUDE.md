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
- `include/swar/packed_set.hpp` — `PackedSet<N>` multi-word set container built on `PackedWord`.
- `test/packed_word_test.cpp` — gtest suite for both types.
- `bench/packed_word_bench.cpp` — gbench suite, template-instantiated for N=5..14.
- `scripts/visualize.py` — Reads gbench JSON from `results/`, produces density and throughput PNGs.

## Conventions

- Header-only C++17 library. All code lives in `include/swar/`.
- Bit-width `N` is always a compile-time template parameter.
- Register type is `uint64_t` only.
- The haszero trick reserves the MSB of each lane as a guard bit. Operations that depend on this (`contains`, `find`, `count_eq`) require values ≤ `max_safe_value` (= 2^(N-1) - 1). Pure storage ops (`get`, `set`, `broadcast`) allow the full N-bit range.
- `PackedSet` uses 0 as the sentinel for empty lanes, so stored values must be ≥ 1.
- Tests use googletest v1.14, benchmarks use google/benchmark v1.8.3, both fetched via CMake FetchContent.
- Python venv is in `venv/` (gitignored). Deps: matplotlib, numpy, pandas.
