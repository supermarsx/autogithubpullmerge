#!/usr/bin/env bash
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/.."
BUILD_DIR="${ROOT_DIR}/build_gpp"
export PKG_CONFIG="$(which pkg-config)"
mkdir -p "$BUILD_DIR"
SRC_FILES=$(find "$ROOT_DIR/src" -name '*.cpp')

CPU_CORES=$(sysctl -n hw.ncpu)

g++ -std=c++20 -Wall -Wextra -O2 -I"${ROOT_DIR}/include" $SRC_FILES \
  $(pkg-config --cflags --libs yaml-cpp libcurl sqlite3 spdlog ncurses) -pthread \
  -o "$BUILD_DIR/autogithubpullmerge"

