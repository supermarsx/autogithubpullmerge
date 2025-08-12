#!/usr/bin/env bash
set -e

if [[ -z "${VCPKG_ROOT}" ]]; then
    echo "[ERROR] VCPKG_ROOT not set. Run install_linux.sh first."
    exit 1
fi

cmake --preset vcpkg
cmake --build --preset vcpkg
ctest --preset vcpkg
