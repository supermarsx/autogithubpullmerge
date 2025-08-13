@echo off

echo ============================================================
echo   AutoGitHubPullMerge Windows Dependency Installation
echo ============================================================
echo.
echo Ensure Microsoft Visual C++ Build Tools are available in your PATH.
echo Run future builds from a Developer Command Prompt where cl is accessible.
echo.
echo [1/5] Installing required packages...
choco install cmake git curl sqlite ninja visualstudio2022buildtools -y

echo [2/5] Determining vcpkg location...
if "%VCPKG_ROOT%"=="" (
    set "VCPKG_ROOT=%~dp0..\vcpkg"
    echo     Using default path %VCPKG_ROOT%
)

echo [3/5] Preparing vcpkg repository...
if not exist "%VCPKG_ROOT%\.vcpkg-root" (
    if exist "%VCPKG_ROOT%" (
        echo     Existing directory found but it is not a valid vcpkg repository. Removing...
        rmdir /s /q "%VCPKG_ROOT%" || exit /b 1
    )
    echo     Cloning vcpkg repository...
    git clone https://github.com/microsoft/vcpkg "%VCPKG_ROOT%" || exit /b 1
)

echo [4/5] Bootstrapping vcpkg...
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" || exit /b 1
)
"%VCPKG_ROOT%\vcpkg.exe" integrate install || exit /b 1

echo [5/5] Installing project dependencies...
if "%VCPKG_DEFAULT_TRIPLET%"=="" set "VCPKG_DEFAULT_TRIPLET=x64-windows-static"
"%VCPKG_ROOT%\vcpkg.exe" install --triplet %VCPKG_DEFAULT_TRIPLET% || exit /b 1

echo Persisting environment variables...
setx VCPKG_ROOT "%VCPKG_ROOT%" /M
setx VCPKG_DEFAULT_TRIPLET "%VCPKG_DEFAULT_TRIPLET%" /M
setx PATH "%PATH%;%VCPKG_ROOT%" /M

echo.
echo All done! Please restart your shell for changes to take effect.

