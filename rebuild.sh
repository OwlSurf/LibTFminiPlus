#!/bin/bash
set -euo pipefail
cmake --build gtest/build
ctest --test-dir gtest/build --output-on-failure
