#!/bin/bash
set -euo pipefail
rm -rf gtest/build
# Prefer GCC when present (avoids clang without libstdc++ in some environments).
export CC="${CC:-gcc}"
export CXX="${CXX:-g++}"
cmake -DCMAKE_BUILD_TYPE=Debug -S gtest/ -B gtest/build
cmake --build gtest/build
ctest --test-dir gtest/build --output-on-failure
