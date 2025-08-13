@echo off
setlocal

:: Ensure MSVC build tools are available
where cl >nul 2>&1
if %errorlevel% neq 0 (
    set "VSWHERE=vswhere"
    where %VSWHERE% >nul 2>&1
    if %errorlevel% neq 0 (
        set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    )
    if not exist "%VSWHERE%" (
        echo [ERROR] vswhere.exe not found. Install Visual Studio Build Tools and try again.
        exit /b 1
    )
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VS_PATH=%%i"
    )
    if not defined VS_PATH (
        echo [ERROR] MSVC build tools not found. Install Visual Studio Build Tools and try again.
        exit /b 1
    )
    call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 || exit /b 1
)

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
cmake -S . -B build\vcpkg -G "Ninja Multi-Config" ^
  -DCMAKE_TOOLCHAIN_FILE=%CD%\vcpkg\scripts\buildsystems\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
  -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl || exit /b 1
cmake --build build\vcpkg --config Release || exit /b 1
ctest --test-dir build\vcpkg -C Release || exit /b 1

endlocal

