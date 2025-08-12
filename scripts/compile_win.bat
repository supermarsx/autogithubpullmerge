@echo off
if "%VCPKG_ROOT%"=="" (
    echo [ERROR] VCPKG_ROOT not set. Run install_win.bat first.
    exit /b 1
)
cmake --preset vcpkg --fresh || exit /b 1
cmake --build --preset vcpkg --config Release || exit /b 1
