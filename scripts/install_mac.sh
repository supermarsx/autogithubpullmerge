#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Install required packages
brew update
brew install cmake git curl libpsl sqlite3 spdlog ncurses pkg-config

# Clone vcpkg if VCPKG_ROOT is not set
if [[ -z "${VCPKG_ROOT}" ]]; then
	git clone https://github.com/microsoft/vcpkg "${SCRIPT_DIR}/../vcpkg"
	export VCPKG_ROOT="${SCRIPT_DIR}/../vcpkg"
fi

# Bootstrap vcpkg if needed
if [[ ! -f "${VCPKG_ROOT}/vcpkg" ]]; then
	"${VCPKG_ROOT}/bootstrap-vcpkg.sh"
fi

# Install dependencies from vcpkg.json
"${VCPKG_ROOT}/vcpkg" install

# Persist environment variables
PROFILE_FILE="$HOME/.zprofile"
if ! grep -q "VCPKG_ROOT" "${PROFILE_FILE}" 2>/dev/null; then
	echo "export VCPKG_ROOT=\"${VCPKG_ROOT}\"" >>"${PROFILE_FILE}"
	echo "export PATH=\"\$VCPKG_ROOT:\$PATH\"" >>"${PROFILE_FILE}"
fi

echo "Environment variables updated. Please restart your shell for changes to take effect."
