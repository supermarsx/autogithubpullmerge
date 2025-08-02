@echo off
setlocal EnableDelayedExpansion

rem Determine libs directory
for %%I in ("%~dp0..") do set "BASE_DIR=%%~fI"
set "LIBS_DIR=!BASE_DIR!\libs"
if not exist "!LIBS_DIR!" mkdir "!LIBS_DIR!"

call :clone_or_update https://github.com/CLIUtils/CLI11.git CLI11
call :clone_or_update https://github.com/jbeder/yaml-cpp.git yaml-cpp
call :clone_or_update https://github.com/yaml/libyaml.git libyaml
call :clone_or_update https://github.com/nlohmann/json.git json
call :clone_or_update https://github.com/gabime/spdlog.git spdlog

rem Fetch prebuilt curl for Windows to obtain libcurl.a
set CURL_VER=8.15.0_4
set CURL_ZIP=curl-%CURL_VER%-win64-mingw.zip
set CURL_URL=https://curl.se/windows/dl-%CURL_VER%/%CURL_ZIP%
set CURL_DIR=!LIBS_DIR!\curl
if not exist "!CURL_DIR!" mkdir "!CURL_DIR!"
if not exist "!CURL_DIR!\lib\libcurl.a" (
    curl -L %CURL_URL% -o "!CURL_DIR!\%CURL_ZIP%" || goto :eof
    powershell -Command "Expand-Archive -Path '!CURL_DIR!\%CURL_ZIP%' -DestinationPath '!CURL_DIR!' -Force" || goto :eof
    move /y "!CURL_DIR!\curl-%CURL_VER%-win64-mingw\*" "!CURL_DIR!" >nul
    rmdir /s /q "!CURL_DIR!\curl-%CURL_VER%-win64-mingw"
    del "!CURL_DIR!\%CURL_ZIP%"
)

set SQLITE_VER=3430000
set SQLITE_YEAR=2023
set SQLITE_ZIP=sqlite-amalgamation-%SQLITE_VER%.zip
set SQLITE_DIR=!LIBS_DIR!\sqlite
if not exist "!SQLITE_DIR!" mkdir "!SQLITE_DIR!"
if not exist "!SQLITE_DIR!\sqlite3.h" (
    curl -L https://sqlite.org/%SQLITE_YEAR%/%SQLITE_ZIP% -o "!SQLITE_DIR!\%SQLITE_ZIP%" || goto :eof
    powershell -Command "Expand-Archive -Path '!SQLITE_DIR!\%SQLITE_ZIP%' -DestinationPath '!SQLITE_DIR!' -Force" || goto :eof
    move "!SQLITE_DIR!\sqlite-amalgamation-%SQLITE_VER%\*" "!SQLITE_DIR!" >nul
    rmdir /s /q "!SQLITE_DIR!\sqlite-amalgamation-%SQLITE_VER%"
    del "!SQLITE_DIR!\%SQLITE_ZIP%"
)

call :clone_or_update https://github.com/wmcbrine/PDCurses.git pdcurses

endlocal
exit /b 0

:clone_or_update
set REPO=%1
set DIR=!LIBS_DIR!\%2
if exist "!DIR!\.git" (
    git -C "!DIR!" pull --ff-only
) else (
    git clone --depth 1 "%REPO%" "!DIR!"
)
exit /b
