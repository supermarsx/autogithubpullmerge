@echo off
if "%VCPKG_ROOT%"=="" (
    echo [ERROR] VCPKG_ROOT not set. Run install_win.bat first.
    exit /b 1
)
rmdir /s /q build\vcpkg 2>nul
cmake -S . -B build\vcpkg -G "Ninja Multi-Config" ^
  -DCMAKE_TOOLCHAIN_FILE=%CD%\vcpkg\scripts\buildsystems\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-mingw-static ^
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ || exit /b 1
cmake --build build\vcpkg --config Release || exit /b 1
 
