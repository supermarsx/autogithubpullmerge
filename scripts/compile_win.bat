@echo off
setlocal

:: Ensure MSVC build tools are available
where cl >nul 2>&1
if %errorlevel% neq 0 (
    rem Locate vswhere using multiple strategies
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
    set "VSWHERE=%VSWHERE_PATH%"
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
    echo [ERROR] VCPKG_ROOT not set. Run install_win.bat first.
    exit /b 1
)
rmdir /s /q build\vcpkg 2>nul
cmake -S . -B build\vcpkg -G "Ninja Multi-Config" ^
  -DCMAKE_TOOLCHAIN_FILE=%CD%\vcpkg\scripts\buildsystems\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
  -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl || exit /b 1
cmake --build build\vcpkg --config Release || exit /b 1

endlocal
