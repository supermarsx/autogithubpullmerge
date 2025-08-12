@echo off
setlocal

if "%VCPKG_ROOT%"=="" (
    if not exist "%~dp0..\vcpkg\.git" (
        git clone https://github.com/microsoft/vcpkg "%~dp0..\vcpkg" || exit /b 1
    )
    set "VCPKG_ROOT=%~dp0..\vcpkg"
)

if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" || exit /b 1
)

"%VCPKG_ROOT%\vcpkg.exe" install || exit /b 1

cmake --preset vcpkg || exit /b 1
cmake --build --preset vcpkg --config Release || exit /b 1
ctest --preset vcpkg || exit /b 1

endlocal
