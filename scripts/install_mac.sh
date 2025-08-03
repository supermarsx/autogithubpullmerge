#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Install required packages including ncurses for the TUI and pkg-config for detection
brew update
brew install cmake git curl libpsl sqlite3 spdlog ncurses pkg-config

# Fetch and build local third-party dependencies into the libs directory
"${SCRIPT_DIR}/update_libs.sh"
