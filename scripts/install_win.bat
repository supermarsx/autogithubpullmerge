@echo off
setlocal

choco install cmake git curl sqlite mingw -y

if "%VCPKG_ROOT%"=="" (
    git clone https://github.com/microsoft/vcpkg "%~dp0..\vcpkg" || exit /b 1
    set "VCPKG_ROOT=%~dp0..\vcpkg"
)

if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" || exit /b 1
)

"%VCPKG_ROOT%\vcpkg.exe" install libev c-ares zlib brotli openssl ngtcp2 nghttp3 jansson libevent libxml2 jemalloc nghttp2[openssl,libevent,tools,http3] --triplet x64-windows || exit /b 1

endlocal
