#!/usr/bin/env bash
set -e

if command -v apt-get >/dev/null 2>&1; then
	apt-get update
	apt-get install -y --no-install-recommends linux-libc-dev
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

if [[ -z "${VCPKG_ROOT}" ]]; then
	if [[ ! -d "${SCRIPT_DIR}/../vcpkg" ]]; then
		git clone https://github.com/microsoft/vcpkg "${SCRIPT_DIR}/../vcpkg"
	fi
	export VCPKG_ROOT="${SCRIPT_DIR}/../vcpkg"
fi

if [[ ! -f "${VCPKG_ROOT}/vcpkg" ]]; then
	"${VCPKG_ROOT}/bootstrap-vcpkg.sh"
fi

"${VCPKG_ROOT}/vcpkg" install

cmake --preset vcpkg
cmake --build --preset vcpkg
ctest --preset vcpkg
