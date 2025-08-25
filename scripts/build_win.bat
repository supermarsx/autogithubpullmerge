@echo off
setlocal

rem Clear any preset compiler variables that may interfere with MSVC detection
set "CC="
set "CXX="
set "CC_COMPILER="
set "CXX_COMPILER="

echo ============================================================
echo   AutoGitHubPullMerge Windows Build and Test
echo ============================================================

echo [1/8] Initializing MSVC environment...
call :init_vcvars

echo [2/8] Checking for MSVC build tools...
where cl >nul 2>&1
if %errorlevel% neq 0 (
    echo     cl.exe not found in PATH. Searching common locations...
    call :find_cl
    if defined CL_PATH (
        call :setup_msvc
    ) else (
        echo     cl.exe not found. Locating via vswhere...
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
        "%VSWHERE_PATH%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%TEMP%\vs_path.txt"
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
)

echo [3/8] Verifying VCPKG_ROOT...
if "%VCPKG_ROOT%"=="" (
    set "SCRIPT_DIR=%~dp0"
    if exist "%SCRIPT_DIR%..\vcpkg\vcpkg.exe" (
        set "VCPKG_ROOT=%SCRIPT_DIR%..\vcpkg"
        echo     Using vcpkg at %VCPKG_ROOT%
    ) else (
        echo [ERROR] VCPKG_ROOT not set and vcpkg not found. Run scripts\install_win.bat first.
        exit /b 1
    )
) else (
    echo     VCPKG_ROOT is %VCPKG_ROOT%
)

echo [4/8] Cleaning previous build directory...
rmdir /s /q build\vcpkg 2>nul

echo [5/8] Configuring project with CMake...
cmake -S . -B build\vcpkg -G "Ninja Multi-Config" ^
  -DCMAKE_TOOLCHAIN_FILE="%CD%\vcpkg\scripts\buildsystems\vcpkg.cmake" ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
  -DCMAKE_CXX_STANDARD=23 ^
  -DCMAKE_C_COMPILER=cl ^
  -DCMAKE_CXX_COMPILER=cl || exit /b 1

echo [6/8] Building project...
cmake --build build\vcpkg --config Release || exit /b 1

echo [7/8] Running tests...
ctest --test-dir build\vcpkg -C Release || exit /b 1

echo [8/8] Build and tests completed successfully.

endlocal
exit /b 0

:init_vcvars
set "VCVARS2022=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\vsdevcmd\ext\vcvars.bat"
set "VCVARS2019=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\vsdevcmd\ext\vcvars.bat"
if exist "%VCVARS2022%" (
    echo     Initializing environment with VC Tools 2022...
    call "%VCVARS2022%" || exit /b 1
) else if exist "%VCVARS2019%" (
    echo     Initializing environment with VC Tools 2019...
    call "%VCVARS2019%" || exit /b 1
) else (
    echo     No vcvars script found in default locations.
)
exit /b 0

:find_cl
set "CL_PATH="
for /f "delims=" %%i in ('dir /b /s "%ProgramFiles(x86)%\Microsoft Visual Studio\*\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe" 2^>nul') do (
    set "CL_PATH=%%i"
    goto :eof
)
for /f "delims=" %%i in ('dir /b /s "%ProgramFiles%\Microsoft Visual Studio\*\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe" 2^>nul') do (
    set "CL_PATH=%%i"
    goto :eof
)
exit /b 0

:setup_msvc
echo     Found cl.exe at %CL_PATH%
for %%i in ("%CL_PATH%") do set "CL_BIN_DIR=%%~dpi"
set "PATH=%CL_BIN_DIR%;%PATH%"
for %%i in ("%CL_BIN_DIR%..\..\..\..\..\..\..") do set "VSINSTALL=%%~fi"
if exist "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" (
    echo     Initializing environment with vcvars64.bat...
    call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
)
exit /b 0

