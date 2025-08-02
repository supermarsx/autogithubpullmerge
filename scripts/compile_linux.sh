#!/usr/bin/env bash
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/.."
BUILD_DIR="${ROOT_DIR}/build_gpp"
mkdir -p "$BUILD_DIR"
SRC_FILES=$(find "$ROOT_DIR/src" -name '*.cpp')
LIBS_DIR="${ROOT_DIR}/libs"

# Dependency locations following the *_install convention
YAMLCPP_INC="${LIBS_DIR}/yaml-cpp/yaml-cpp_install/include"
YAMLCPP_LIB="${LIBS_DIR}/yaml-cpp/yaml-cpp_install/lib"
CURL_INC="${LIBS_DIR}/curl/curl_install/include"
CURL_LIB="${LIBS_DIR}/curl/curl_install/lib"

# Ensure headers exist for critical dependencies
[[ -f "${YAMLCPP_INC}/yaml-cpp/yaml.h" ]] || {
    echo "[ERROR] yaml-cpp headers not found at ${YAMLCPP_INC}" >&2
    exit 1
}
[[ -f "${CURL_INC}/curl/curl.h" ]] || {
    echo "[ERROR] curl headers not found at ${CURL_INC}" >&2
    exit 1
}

INCLUDE_FLAGS="-I\"${ROOT_DIR}/include\" -I\"${LIBS_DIR}/CLI11/include\" -I\"${LIBS_DIR}/json/include\" -I\"${LIBS_DIR}/spdlog/include\" -I\"${YAMLCPP_INC}\" -I\"${CURL_INC}\" -I\"${LIBS_DIR}/sqlite\""

[[ -f "${YAMLCPP_LIB}/libyaml-cpp.a" ]] || {
    echo "[ERROR] libyaml-cpp.a not found at ${YAMLCPP_LIB}" >&2
    exit 1
}
[[ -f "${CURL_LIB}/libcurl.a" ]] || {
    echo "[ERROR] libcurl.a not found at ${CURL_LIB}" >&2
    exit 1
}

g++ -std=c++20 -Wall -Wextra -O2 $INCLUDE_FLAGS $SRC_FILES \
    -L"${YAMLCPP_LIB}" -lyaml-cpp \
    -L"${CURL_LIB}" -lcurl \
    $(pkg-config --cflags --libs sqlite3 ncurses) -pthread \
    -o "$BUILD_DIR/autogithubpullmerge"
