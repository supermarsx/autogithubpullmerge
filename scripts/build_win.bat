@echo off
setlocal
set "BUILD_DIR=build"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

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
        where make >nul 2>&1
        if %errorlevel%==0 (
            set "GEN=Unix Makefiles"
            set "BUILD_CMD=cmake --build ."
        ) else (
            echo [ERROR] No suitable build tool found. Install nmake, mingw32-make or make.
            exit /b 1
        )
    )
)

cmake .. -G "%GEN%" -DBUILD_SHARED_LIBS=OFF || exit /b 1
%BUILD_CMD% || exit /b 1
ctest || exit /b 1
endlocal
