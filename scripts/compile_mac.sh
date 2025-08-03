#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/.."
BUILD_DIR="${ROOT_DIR}/build_gpp"
LIBS_DIR="${ROOT_DIR}/libs"

# Ensure local dependencies are present; if missing, fetch and build them
if [[ ! -f "${LIBS_DIR}/yaml-cpp/yaml-cpp_install/lib/libyaml-cpp.a" || ! -f "${LIBS_DIR}/curl/curl_install/lib/libcurl.a" ]]; then
	"${SCRIPT_DIR}/update_libs.sh"
fi

mkdir -p "$BUILD_DIR"

mapfile -t SRC_FILES < <(find "$ROOT_DIR/src" -name '*.cpp')

# Dependency locations following the *_install convention
YAMLCPP_INC="${LIBS_DIR}/yaml-cpp/yaml-cpp_install/include"
YAMLCPP_LIB="${LIBS_DIR}/yaml-cpp/yaml-cpp_install/lib"
CURL_INC="${LIBS_DIR}/curl/curl_install/include"
CURL_LIB="${LIBS_DIR}/curl/curl_install/lib"

# Ensure required headers exist
[[ -f "${YAMLCPP_INC}/yaml-cpp/yaml.h" ]] || {
	echo "[ERROR] yaml-cpp headers not found at ${YAMLCPP_INC}" >&2
	exit 1
}
[[ -f "${CURL_INC}/curl/curl.h" ]] || {
	echo "[ERROR] curl headers not found at ${CURL_INC}" >&2
	exit 1
}

# Include directories for project headers and third-party libraries
INCLUDE_FLAGS=(
	"-I${ROOT_DIR}/include"
	"-I${LIBS_DIR}/CLI11/include"
	"-I${LIBS_DIR}/json/include"
	"-I${LIBS_DIR}/spdlog/include"
	"-I${YAMLCPP_INC}"
	"-I${CURL_INC}"
	"-I${LIBS_DIR}/sqlite"
)

if command -v pkg-config >/dev/null; then
	mapfile -t PKG_FLAGS < <(pkg-config --cflags --libs sqlite3 ncurses libpsl)
else
	echo "pkg-config not found, using fallback flags"
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

g++ -std=c++20 -Wall -Wextra -O2 "${INCLUDE_FLAGS[@]}" "${SRC_FILES[@]}" \
	-L"${YAMLCPP_LIB}" -lyaml-cpp \
	-L"${CURL_LIB}" -lcurl \
	"${PKG_FLAGS[@]}" -pthread \
	-o "$BUILD_DIR/autogithubpullmerge"
