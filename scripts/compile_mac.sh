#!/usr/bin/env bash
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/.."
BUILD_DIR="${ROOT_DIR}/build_gpp"
mkdir -p "$BUILD_DIR"
SRC_FILES=$(find "$ROOT_DIR/src" -name '*.cpp')
LIBS_DIR="${ROOT_DIR}/libs"
INCLUDE_FLAGS="-I\"${ROOT_DIR}/include\" -I\"${LIBS_DIR}/CLI11/include\" -I\"${LIBS_DIR}/json/include\" -I\"${LIBS_DIR}/spdlog/include\" -I\"${LIBS_DIR}/yaml-cpp/include\""

CPU_CORES=$(sysctl -n hw.ncpu)

g++ -std=c++20 -Wall -Wextra -O2 $INCLUDE_FLAGS $SRC_FILES \
  $(pkg-config --cflags --libs yaml-cpp libcurl sqlite3 ncurses) -pthread \
  -o "$BUILD_DIR/autogithubpullmerge"

