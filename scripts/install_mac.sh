#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "✨ Beginning macOS dependency installation..."

# Install required packages
echo "🍎 Updating Homebrew and installing requirements..."
if ! command -v brew >/dev/null 2>&1; then
        echo "❌ Homebrew is not installed. Please install it from https://brew.sh and rerun this script."
        exit 1
fi
brew update
brew install cmake git curl libpsl sqlite3 spdlog ncurses pkg-config

# Clone vcpkg if VCPKG_ROOT is not set
echo "📦 Ensuring vcpkg is available..."
if [[ -z "${VCPKG_ROOT}" ]]; then
	echo "📥 Cloning vcpkg into ${SCRIPT_DIR}/../vcpkg"
	git clone https://github.com/microsoft/vcpkg "${SCRIPT_DIR}/../vcpkg"
	export VCPKG_ROOT="${SCRIPT_DIR}/../vcpkg"
fi

# Bootstrap vcpkg if needed
echo "🚀 Bootstrapping vcpkg..."
if [[ ! -f "${VCPKG_ROOT}/vcpkg" ]]; then
	"${VCPKG_ROOT}/bootstrap-vcpkg.sh"
fi

# Install dependencies from vcpkg.json
echo "📚 Installing project dependencies via vcpkg..."
"${VCPKG_ROOT}/vcpkg" install

# Persist environment variables
echo "📝 Persisting VCPKG_ROOT and updating PATH..."
PROFILE_FILE="$HOME/.zprofile"
if ! grep -q "VCPKG_ROOT" "${PROFILE_FILE}" 2>/dev/null; then
	echo "export VCPKG_ROOT=\"${VCPKG_ROOT}\"" >>"${PROFILE_FILE}"
	echo "export PATH=\"\$VCPKG_ROOT:\$PATH\"" >>"${PROFILE_FILE}"
fi

echo "✅ Installation completed. Please restart your shell for changes to take effect."
