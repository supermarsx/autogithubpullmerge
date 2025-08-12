#!/usr/bin/env bash
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/.."
BUILD_DIR="${ROOT_DIR}/build_gpp"
mkdir -p "$BUILD_DIR"
mapfile -t SRC_FILES < <(find "$ROOT_DIR/src" -name '*.cpp')
LIBS_DIR="${ROOT_DIR}/libs"

# Dependency locations following the *_install convention
YAMLCPP_INC="${LIBS_DIR}/yaml-cpp/yaml-cpp_install/include"
YAMLCPP_LIB="${LIBS_DIR}/yaml-cpp/yaml-cpp_install/lib"
CURL_INC="${LIBS_DIR}/curl/curl_install/include"
CURL_LIB="${LIBS_DIR}/curl/curl_install/lib"
NGHTTP2_INC="${LIBS_DIR}/nghttp2/nghttp2_install/include"
NGHTTP2_LIB="${LIBS_DIR}/nghttp2/nghttp2_install/lib"

# Ensure headers exist for critical dependencies
[[ -f "${YAMLCPP_INC}/yaml-cpp/yaml.h" ]] || {
	echo "[ERROR] yaml-cpp headers not found at ${YAMLCPP_INC}" >&2
	exit 1
}
[[ -f "${CURL_INC}/curl/curl.h" ]] || {
        echo "[ERROR] curl headers not found at ${CURL_INC}" >&2
        exit 1
}
[[ -f "${NGHTTP2_INC}/nghttp2/nghttp2.h" ]] || {
        echo "[ERROR] nghttp2 headers not found at ${NGHTTP2_INC}" >&2
        exit 1
}

INCLUDE_FLAGS=(
	"-I${ROOT_DIR}/include"
	"-I${LIBS_DIR}/CLI11/include"
	"-I${LIBS_DIR}/json/include"
	"-I${LIBS_DIR}/spdlog/include"
        "-I${YAMLCPP_INC}"
        "-I${CURL_INC}"
        "-I${NGHTTP2_INC}"
        "-I${LIBS_DIR}/sqlite"
)

if command -v pkg-config >/dev/null; then
	mapfile -t PKG_FLAGS < <(pkg-config --cflags --libs sqlite3 ncurses libpsl)
else
	PKG_FLAGS=(-lsqlite3 -lncurses -lpsl)
fi

[[ -f "${YAMLCPP_LIB}/libyaml-cpp.a" ]] || {
	echo "[ERROR] libyaml-cpp.a not found at ${YAMLCPP_LIB}" >&2
	exit 1
}
[[ -f "${CURL_LIB}/libcurl.a" ]] || {
        echo "[ERROR] libcurl.a not found at ${CURL_LIB}" >&2
        exit 1
}
[[ -f "${NGHTTP2_LIB}/libnghttp2.a" ]] || {
        echo "[ERROR] libnghttp2.a not found at ${NGHTTP2_LIB}" >&2
        exit 1
}

g++ -std=c++20 -Wall -Wextra -O2 "${INCLUDE_FLAGS[@]}" "${SRC_FILES[@]}" \
        -L"${YAMLCPP_LIB}" -lyaml-cpp \
        -L"${CURL_LIB}" -lcurl \
        -L"${NGHTTP2_LIB}" -lnghttp2 \
        "${PKG_FLAGS[@]}" -pthread \
        -o "$BUILD_DIR/autogithubpullmerge"
