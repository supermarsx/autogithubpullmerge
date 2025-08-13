@echo off
setlocal

echo ============================================================
echo   AutoGitHubPullMerge Windows Compilation
echo ============================================================

echo [1/6] Checking for MSVC build tools...
where cl >nul 2>&1
if %errorlevel% neq 0 (
    echo     cl.exe not found in PATH. Locating via vswhere...
    set "VSWHERE_PATH="
    if defined VSWHERE if exist "%VSWHERE%" set "VSWHERE_PATH=%VSWHERE%"
    if not defined VSWHERE_PATH (
        for %%i in (vswhere.exe) do if exist "%%~$PATH:i" set "VSWHERE_PATH=%%~$PATH:i"
    )
    if not defined VSWHERE_PATH if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" set "VSWHERE_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not defined VSWHERE_PATH if exist "%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe" set "VSWHERE_PATH=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not defined VSWHERE_PATH if defined ChocolateyInstall if exist "%ChocolateyInstall%\bin\vswhere.exe" set "VSWHERE_PATH=%ChocolateyInstall%\bin\vswhere.exe"
    if not defined VSWHERE_PATH (
        echo [ERROR] vswhere.exe not found. Install Visual Studio Build Tools and try again.
        exit /b 1
    )
    echo     vswhere found at %VSWHERE_PATH%
    for %%i in ("%VSWHERE_PATH%") do set "VSWHERE_SHORT=%%~fsi"
    "%VSWHERE_SHORT%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%TEMP%\vs_path.txt"
    set /p VS_PATH=<"%TEMP%\vs_path.txt"
    del "%TEMP%\vs_path.txt"
    if not defined VS_PATH (
        echo [ERROR] MSVC build tools not found. Install Visual Studio Build Tools and try again.
        exit /b 1
    )
    echo     Using Visual Studio tools at %VS_PATH%
    echo     Initializing environment with VsDevCmd.bat...
    call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 || exit /b 1
)

echo [2/6] Verifying VCPKG_ROOT...
if "%VCPKG_ROOT%"=="" (
    echo [ERROR] VCPKG_ROOT not set. Run install_win.bat first.
    exit /b 1
) else (
    echo     VCPKG_ROOT is %VCPKG_ROOT%
)

echo [3/6] Cleaning previous build directory...
rmdir /s /q build\vcpkg 2>nul

echo [4/6] Configuring project with CMake...
cmake -S . -B build\vcpkg -G "Ninja Multi-Config" ^
  -DCMAKE_TOOLCHAIN_FILE=%CD%\vcpkg\scripts\buildsystems\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
  -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl || exit /b 1

echo [5/6] Building project...
cmake --build build\vcpkg --config Release || exit /b 1

echo [6/6] Compilation complete.

endlocal
