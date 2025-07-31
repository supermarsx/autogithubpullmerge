#!/usr/bin/env bash
set -e
BUILD_DIR="build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DBUILD_SHARED_LIBS=OFF
cmake --build . -- -j$(nproc)
ctest
