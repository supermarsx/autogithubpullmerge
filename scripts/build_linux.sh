#!/usr/bin/env bash
set -e

cmake --preset vcpkg
cmake --build --preset vcpkg
ctest --preset vcpkg
