@echo off
setlocal EnableDelayedExpansion

rem ---------------------------------------------------------------------------
rem  Determine project root
rem ---------------------------------------------------------------------------
for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
cd /d "%ROOT_DIR%" || exit /b 1

rem ---------------------------------------------------------------------------
rem  Prepare build directories
rem ---------------------------------------------------------------------------
set "BUILD_DIR=%ROOT_DIR%\build_gpp"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

rem ---------------------------------------------------------------------------
rem  Collect sources (add sqlite amalgamation manually)
rem ---------------------------------------------------------------------------
set SRC_LIST=
for /r "%ROOT_DIR%\src" %%F in (*.cpp) do (
    set SRC_LIST=!SRC_LIST! "%%F"
)
if exist "%ROOT_DIR%\libs\sqlite\sqlite3.c" (
    set SRC_LIST=!SRC_LIST! "%ROOT_DIR%\libs\sqlite\sqlite3.c"
)

rem ---------------------------------------------------------------------------
rem  Include directories
rem ---------------------------------------------------------------------------
set "LIBS_DIR=%ROOT_DIR%\libs"
set INCLUDE_ARGS=^
  -I"%ROOT_DIR%\include" ^
  -I"%LIBS_DIR%\CLI11\include" ^
  -I"%LIBS_DIR%\json\include" ^
  -I"%LIBS_DIR%\spdlog\include" ^
  -I"%LIBS_DIR%\yaml-cpp\include" ^
  -I"%LIBS_DIR%\pdcurses" ^
  -I"%LIBS_DIR%\curl\include" ^
  -I"%LIBS_DIR%\sqlite"

rem ---------------------------------------------------------------------------
rem  Library locations (expect static libs in these folders)
rem ---------------------------------------------------------------------------
set "CURL_LIB=%LIBS_DIR%\curl\lib\libcurl.a"
set "YAMLCPP_LIB=%LIBS_DIR%\yaml-cpp\lib\libyaml-cpp.a"
set "PDCURSES_LIB=%LIBS_DIR%\pdcurses\lib\pdcurses.a"
set LIB_ARGS="%CURL_LIB%" "%YAMLCPP_LIB%" "%PDCURSES_LIB%"

if not exist "%CURL_LIB%" (
    echo [ERROR] libcurl.a not found at %CURL_LIB%
    exit /b 1
)
if not exist "%YAMLCPP_LIB%" (
    echo [ERROR] libyaml-cpp.a not found at %YAMLCPP_LIB%
    exit /b 1
)
if not exist "%PDCURSES_LIB%" (
    echo [ERROR] pdcurses.a not found at %PDCURSES_LIB%
    exit /b 1
)

rem ---------------------------------------------------------------------------
rem  Compile
rem ---------------------------------------------------------------------------
g++ -std=c++20 -Wall -Wextra -O2 -static -static-libgcc -static-libstdc++ -DYAML_CPP_STATIC_DEFINE %INCLUDE_ARGS% %SRC_LIST% %LIB_ARGS% -o "%BUILD_DIR%\autogithubpullmerge.exe"

endlocal

