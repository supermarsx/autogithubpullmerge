#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=util.sh
. "${SCRIPT_DIR}/util.sh"

ensure_tool make
ensure_tool cmake

if [[ -z "${VCPKG_ROOT}" ]]; then
	echo "[ERROR] VCPKG_ROOT not set. Run install_mac.sh first."
	exit 1
fi

cmake --preset vcpkg --fresh -DBUILD_TESTING=OFF
cmake --build --preset vcpkg --target autogithubpullmerge
