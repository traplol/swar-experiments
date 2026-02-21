#!/bin/bash
# Build and run tests, print only PASS or FAIL.
cd "$(dirname "$0")/.."
cmake --build build -j$(nproc) >/dev/null 2>&1 && ./build/swar_tests --gtest_brief=1 >/dev/null 2>&1
if [ $? -eq 0 ]; then echo "PASS"; else echo "FAIL"; fi
