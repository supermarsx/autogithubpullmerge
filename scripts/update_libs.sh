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
clone_or_update https://github.com/sqlite/sqlite.git sqlite
# Rename VERSION file to avoid clashing with C++ <version> header
[ -f "$LIBS_DIR/sqlite/VERSION" ] && mv "$LIBS_DIR/sqlite/VERSION" "$LIBS_DIR/sqlite/VERSION.txt"
clone_or_update https://github.com/mirror/ncurses.git ncurses
