@echo off
setlocal

if "%VCPKG_ROOT%"=="" (
    set "SCRIPT_DIR=%~dp0"
    if exist "%SCRIPT_DIR%..\vcpkg\vcpkg.exe" (
        set "VCPKG_ROOT=%SCRIPT_DIR%..\vcpkg"
    ) else (
        echo [ERROR] VCPKG_ROOT not set and vcpkg not found. Run scripts\install_win.bat first.
        exit /b 1
    )
)

cmake --preset vcpkg --fresh || exit /b 1
cmake --build --preset vcpkg --config Release || exit /b 1
ctest --preset vcpkg || exit /b 1

endlocal
