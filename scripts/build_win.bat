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

rmdir /s /q build\vcpkg 2>nul
echo %PATH% | findstr /I "C:\\Strawberry\\c\\bin" >nul
if %ERRORLEVEL%==0 (
    echo [WARNING] C:\Strawberry\c\bin found in PATH. Move it after MSVC tools.
)
cmake -S . -B build\vcpkg -G "Ninja Multi-Config" ^
  -DCMAKE_TOOLCHAIN_FILE=%CD%\vcpkg\scripts\buildsystems\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows ^
  -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl || exit /b 1
cmake --build build\vcpkg --config Release || exit /b 1
ctest --test-dir build\vcpkg -C Release || exit /b 1

endlocal

