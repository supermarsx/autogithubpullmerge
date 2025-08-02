#!/usr/bin/env bash
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LIBS_DIR="${SCRIPT_DIR}/../libs"
mkdir -p "$LIBS_DIR"

clone_or_update() {
	local repo="$1"
	local dir="$LIBS_DIR/$2"
	if [ -d "$dir/.git" ]; then
		git -C "$dir" pull --ff-only
	else
		git clone --depth 1 "$repo" "$dir"
	fi
}

clone_or_update https://github.com/CLIUtils/CLI11.git CLI11
clone_or_update https://github.com/jbeder/yaml-cpp.git yaml-cpp
clone_or_update https://github.com/yaml/libyaml.git libyaml
clone_or_update https://github.com/nlohmann/json.git json
clone_or_update https://github.com/gabime/spdlog.git spdlog
clone_or_update https://github.com/curl/curl.git curl

# Build and install yaml-cpp into a local install directory
YAMLCPP_SRC="$LIBS_DIR/yaml-cpp"
YAMLCPP_INSTALL="$YAMLCPP_SRC/yaml-cpp_install"
if [ ! -f "$YAMLCPP_INSTALL/lib/libyaml-cpp.a" ]; then
    cmake -S "$YAMLCPP_SRC" -B "$YAMLCPP_SRC/build" -DBUILD_SHARED_LIBS=OFF -DYAML_CPP_BUILD_TESTS=OFF \
        -DCMAKE_INSTALL_PREFIX="$YAMLCPP_INSTALL"
    cmake --build "$YAMLCPP_SRC/build" --config Release
    cmake --install "$YAMLCPP_SRC/build"
fi

# Build and install curl into a local install directory
CURL_SRC="$LIBS_DIR/curl"
CURL_INSTALL="$CURL_SRC/curl_install"
if [ ! -f "$CURL_INSTALL/lib/libcurl.a" ]; then
    cmake -S "$CURL_SRC" -B "$CURL_SRC/build" -DBUILD_SHARED_LIBS=OFF -DBUILD_CURL_EXE=OFF \
        -DCMAKE_INSTALL_PREFIX="$CURL_INSTALL"
    cmake --build "$CURL_SRC/build" --config Release
    cmake --install "$CURL_SRC/build"
fi

# Download SQLite amalgamation containing sqlite3.c and sqlite3.h
SQLITE_VER=3430000
SQLITE_YEAR=2023
SQLITE_ZIP="sqlite-amalgamation-${SQLITE_VER}.zip"
SQLITE_DIR="$LIBS_DIR/sqlite"
mkdir -p "$SQLITE_DIR"
if [ ! -f "$SQLITE_DIR/sqlite3.h" ]; then
    curl -L "https://sqlite.org/${SQLITE_YEAR}/${SQLITE_ZIP}" -o "$SQLITE_DIR/${SQLITE_ZIP}"
    unzip -oq "$SQLITE_DIR/${SQLITE_ZIP}" -d "$SQLITE_DIR"
    mv "$SQLITE_DIR/sqlite-amalgamation-${SQLITE_VER}"/* "$SQLITE_DIR"
    rmdir "$SQLITE_DIR/sqlite-amalgamation-${SQLITE_VER}"
    rm "$SQLITE_DIR/${SQLITE_ZIP}"
fi

clone_or_update https://github.com/wmcbrine/PDCurses.git pdcurses
