#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "✨ Beginning Linux dependency installation..."

# Install essential build tools
echo "🔧 Updating package lists and installing build essentials..."
sudo apt-get update
sudo apt-get install -y build-essential cmake git curl pkg-config ninja-build libpsl-dev libsqlite3-dev libspdlog-dev libncurses-dev

echo "📦 Ensuring vcpkg is available..."
# Respect existing VCPKG_ROOT if provided; otherwise default to repo-local vcpkg
TARGET_VCPKG_ROOT="${VCPKG_ROOT:-${SCRIPT_DIR}/../vcpkg}"
if [[ ! -d "${TARGET_VCPKG_ROOT}" ]] || [[ ! -f "${TARGET_VCPKG_ROOT}/bootstrap-vcpkg.sh" && ! -f "${TARGET_VCPKG_ROOT}/vcpkg" ]]; then
  echo "📥 Cloning vcpkg into ${TARGET_VCPKG_ROOT}"
  mkdir -p "${TARGET_VCPKG_ROOT}"
  if [[ -d "${TARGET_VCPKG_ROOT}/.git" ]]; then
    echo "ℹ️  ${TARGET_VCPKG_ROOT} already looks like a git repo; skipping clone"
  else
    git clone https://github.com/microsoft/vcpkg "${TARGET_VCPKG_ROOT}"
  fi
fi
export VCPKG_ROOT="${TARGET_VCPKG_ROOT}"

echo "🚀 Bootstrapping vcpkg..."
if [[ ! -f "${VCPKG_ROOT}/vcpkg" ]]; then
  "${VCPKG_ROOT}/bootstrap-vcpkg.sh"
fi

echo "🎯 Setting up default vcpkg triplet..."
if [[ -z "${VCPKG_DEFAULT_TRIPLET}" ]]; then
	export VCPKG_DEFAULT_TRIPLET="x64-linux"
fi

echo "📚 Installing project dependencies via vcpkg..."
"${VCPKG_ROOT}/vcpkg" install --triplet "${VCPKG_DEFAULT_TRIPLET}"
"${VCPKG_ROOT}/vcpkg" integrate install

echo "📝 Persisting VCPKG_ROOT, VCPKG_DEFAULT_TRIPLET and updating PATH..."
PROFILE_FILE="$HOME/.bashrc"
if ! grep -q "VCPKG_ROOT" "${PROFILE_FILE}"; then
	{
		echo "export VCPKG_ROOT=\"${VCPKG_ROOT}\""
		echo "export VCPKG_DEFAULT_TRIPLET=\"${VCPKG_DEFAULT_TRIPLET}\""
		echo "export PATH=\"\$VCPKG_ROOT:\$PATH\""
	} >>"${PROFILE_FILE}"
fi

echo "✅ Installation completed. Please restart your shell for changes to take effect."
