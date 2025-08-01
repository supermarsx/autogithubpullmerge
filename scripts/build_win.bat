@echo off
setlocal
set "BUILD_DIR=build"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"
cmake .. -G "NMake Makefiles" -DBUILD_SHARED_LIBS=OFF || exit /b 1
cmake --build . -- /M || exit /b 1
ctest || exit /b 1
endlocal
