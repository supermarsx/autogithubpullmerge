#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/.."
BUILD_DIR="${ROOT_DIR}/build_gpp"
mkdir -p "$BUILD_DIR"

SRC_FILES=$(find "$ROOT_DIR/src" -name '*.cpp')
LIBS_DIR="${ROOT_DIR}/libs"

# Include directories for project headers and third-party libraries
INCLUDE_FLAGS=(
    "-I${ROOT_DIR}/include"
    "-I${LIBS_DIR}/CLI11/include"
    "-I${LIBS_DIR}/json/include"
    "-I${LIBS_DIR}/spdlog/include"
    "-I${LIBS_DIR}/yaml-cpp/include"
    "-I${LIBS_DIR}/libyaml/include"
    "-I${LIBS_DIR}/ncurses/include"
    "-I${LIBS_DIR}/curl/include"
)

if command -v pkg-config >/dev/null; then
    PKG_FLAGS=$(pkg-config --cflags --libs yaml-cpp yaml-0.1 libcurl sqlite3 ncurses)
else
    echo "pkg-config not found, using fallback flags"
    PKG_FLAGS="-lyaml-cpp -lyaml -lcurl -lsqlite3 -lncurses"
fi

CPU_CORES=$(sysctl -n hw.ncpu)

# shellcheck disable=SC2068
g++ -std=c++20 -Wall -Wextra -O2 ${INCLUDE_FLAGS[@]} $SRC_FILES $PKG_FLAGS -pthread \
    -o "$BUILD_DIR/autogithubpullmerge"
