cmake_minimum_required(VERSION 3.16)
project(swar-experiments LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Header-only library
add_library(swar INTERFACE)
target_include_directories(swar INTERFACE ${CMAKE_SOURCE_DIR}/include)

# ---------- Dependencies via FetchContent ----------
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.8.3
)

# Prevent benchmark from building its own tests
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest googlebenchmark)

# ---------- Tests ----------
enable_testing()

add_executable(swar_tests test/packed_word_test.cpp)
target_link_libraries(swar_tests PRIVATE swar GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(swar_tests)

# ---------- Benchmarks ----------
add_executable(swar_bench bench/packed_word_bench.cpp)
target_link_libraries(swar_bench PRIVATE swar benchmark::benchmark_main)
