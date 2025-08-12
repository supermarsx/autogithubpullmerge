@echo off
setlocal

if "%VCPKG_ROOT%"=="" (
    echo [ERROR] VCPKG_ROOT not set. Run install_win.bat first.
    exit /b 1
)

cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows -DBUILD_SHARED_LIBS=OFF -DENABLE_SYSTEMD=OFF || exit /b 1
cmake --build build --config Release || exit /b 1
ctest --test-dir build -C Release || exit /b 1

endlocal

