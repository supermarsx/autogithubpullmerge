@echo off
cmake --preset vcpkg || exit /b 1
cmake --build --preset vcpkg --config Release || exit /b 1
