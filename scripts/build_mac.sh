#!/usr/bin/env bash
set -e

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
