@echo off

choco install cmake git curl sqlite mingw -y

if "%VCPKG_ROOT%"=="" (
    set "VCPKG_ROOT=%~dp0..\vcpkg"
)

if not exist "%VCPKG_ROOT%\.vcpkg-root" (
    if exist "%VCPKG_ROOT%" (
        echo Existing vcpkg directory found but it is not a valid vcpkg repository. Removing...
        rmdir /s /q "%VCPKG_ROOT%" || exit /b 1
    )
    git clone https://github.com/microsoft/vcpkg "%VCPKG_ROOT%" || exit /b 1
)

if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" || exit /b 1
)
"%VCPKG_ROOT%\vcpkg.exe" integrate install || exit /b 1

if "%VCPKG_DEFAULT_TRIPLET%"=="" set "VCPKG_DEFAULT_TRIPLET=x64-windows"
"%VCPKG_ROOT%\vcpkg.exe" install --triplet %VCPKG_DEFAULT_TRIPLET% || exit /b 1

setx VCPKG_ROOT "%VCPKG_ROOT%" /M
setx VCPKG_DEFAULT_TRIPLET "%VCPKG_DEFAULT_TRIPLET%" /M
setx PATH "%PATH%;%VCPKG_ROOT%" /M
echo Environment variables updated. Please restart your shell for changes to take effect.
