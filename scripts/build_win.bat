@echo off
setlocal
set "BUILD_DIR=build"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

if "%VCPKG_ROOT%"=="" (
    echo [ERROR] VCPKG_ROOT not set. Run install_win.bat first.
    exit /b 1
)
set "VCPKG_TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"

where nmake >nul 2>&1
if %errorlevel%==0 (
    set "GEN=NMake Makefiles"
    set "BUILD_CMD=cmake --build . -- /M"
) else (
    where mingw32-make >nul 2>&1
    if %errorlevel%==0 (
        set "GEN=MinGW Makefiles"
        set "BUILD_CMD=cmake --build ."
    ) else (
        where cmake >nul 2>&1
        if %errorlevel%==0 (
            set "GEN=Unix Makefiles"
            set "BUILD_CMD=make --build ../"
        ) else (
            echo [ERROR] No suitable build tool found. Install nmake, mingw32-make or make.
            exit /b 1
        )
    )
)

cmake .. -G "%GEN%" -DBUILD_SHARED_LIBS=OFF -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%" -DVCPKG_TARGET_TRIPLET=x64-windows -DENABLE_SYSTEMD=OFF || exit /b 1
%BUILD_CMD% || exit /b 1
ctest || exit /b 1
endlocal
