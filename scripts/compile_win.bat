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
set SQLITE_OBJ=
if exist "%ROOT_DIR%\libs\sqlite\sqlite3.c" (
    g++ -x c -O2 -I"%ROOT_DIR%\libs\sqlite" -c "%ROOT_DIR%\libs\sqlite\sqlite3.c" -o "%BUILD_DIR%\sqlite3.o" || exit /b 1
    set SQLITE_OBJ="%BUILD_DIR%\sqlite3.o"
)

rem ---------------------------------------------------------------------------
rem  Include directories
rem ---------------------------------------------------------------------------
set "LIBS_DIR=%ROOT_DIR%\libs"

rem Dependency include and lib directories produced by their install steps
set "YAMLCPP_INC=%LIBS_DIR%\yaml-cpp\yaml-cpp_install\include"
set "YAMLCPP_LIB_A=%LIBS_DIR%\yaml-cpp\yaml-cpp_install\lib\libyaml-cpp.a"
set "YAMLCPP_LIB_DLL_A=%LIBS_DIR%\yaml-cpp\yaml-cpp_install\lib\libyaml-cpp.dll.a"
set "YAMLCPP_LIB_LIB=%LIBS_DIR%\yaml-cpp\yaml-cpp_install\lib\yaml-cpp.lib"
set "CURL_INC=%LIBS_DIR%\curl\curl_install\include"
set "CURL_LIB_A=%LIBS_DIR%\curl\curl_install\lib\libcurl.a"
set "CURL_LIB_DLL_A=%LIBS_DIR%\curl\curl_install\lib\libcurl.dll.a"
set "CURL_LIB_LIB=%LIBS_DIR%\curl\curl_install\lib\curl.lib"
set "PDCURSES_INC=%LIBS_DIR%\pdcurses\pdcurses_install\include"
set "PDCURSES_LIB_A=%LIBS_DIR%\pdcurses\pdcurses_install\lib\pdcurses.a"
set "PDCURSES_LIB_LIB=%LIBS_DIR%\pdcurses\pdcurses_install\lib\pdcurses.lib"

rem Verify headers exist for required dependencies
if not exist "%YAMLCPP_INC%\yaml-cpp\yaml.h" (
    echo [ERROR] yaml-cpp headers not found at %YAMLCPP_INC%
    exit /b 1
)
if not exist "%CURL_INC%\curl\curl.h" (
    echo [ERROR] curl headers not found at %CURL_INC%
    exit /b 1
)
if not exist "%PDCURSES_INC%\curses.h" (
    echo [ERROR] pdcurses headers not found at %PDCURSES_INC%
    exit /b 1
)

if exist "%PDCURSES_LIB_A%" (
    set "PDCURSES_LIB=%PDCURSES_LIB_A%"
) else if exist "%PDCURSES_LIB_LIB%" (
    set "PDCURSES_LIB=%PDCURSES_LIB_LIB%"
) else (
    echo [ERROR] pdcurses library not found at %PDCURSES_LIB_A% or %PDCURSES_LIB_LIB%
    exit /b 1
)

if exist "%YAMLCPP_LIB_A%" (
    set "YAMLCPP_LIB=%YAMLCPP_LIB_A%"
) else if exist "%YAMLCPP_LIB_DLL_A%" (
    set "YAMLCPP_LIB=%YAMLCPP_LIB_DLL_A%"
) else if exist "%YAMLCPP_LIB_LIB%" (
    set "YAMLCPP_LIB=%YAMLCPP_LIB_LIB%"
) else (
    echo [ERROR] yaml-cpp library not found at %YAMLCPP_LIB_A% or %YAMLCPP_LIB_DLL_A% or %YAMLCPP_LIB_LIB%
    exit /b 1
)

if exist "%CURL_LIB_A%" (
    set "CURL_LIB=%CURL_LIB_A%"
    set "CURL_STATIC_DEF=-DCURL_STATICLIB"
) else if exist "%CURL_LIB_DLL_A%" (
    set "CURL_LIB=%CURL_LIB_DLL_A%"
) else if exist "%CURL_LIB_LIB%" (
    set "CURL_LIB=%CURL_LIB_LIB%"
) else (
    echo [ERROR] curl library not found at %CURL_LIB_A% or %CURL_LIB_DLL_A% or %CURL_LIB_LIB%
    exit /b 1
)

set INCLUDE_ARGS=^
  -I"%ROOT_DIR%\include" ^
  -I"%LIBS_DIR%\CLI11\include" ^
  -I"%LIBS_DIR%\json\include" ^
  -I"%LIBS_DIR%\spdlog\include" ^
  -I"%YAMLCPP_INC%" ^
  -I"%PDCURSES_INC%" ^
  -I"%CURL_INC%" ^
  -I"%LIBS_DIR%\sqlite"

rem ---------------------------------------------------------------------------
rem ---------------------------------------------------------------------------
rem  Library locations (expect static libs in these folders)
rem ---------------------------------------------------------------------------
set LIB_ARGS="%CURL_LIB%" "%YAMLCPP_LIB%" "%PDCURSES_LIB%" -lws2_32 -lwinmm -lbcrypt -lcrypt32 -lwldap32

if not exist "%CURL_LIB%" (
    echo [ERROR] curl library not found at %CURL_LIB%
    exit /b 1
)
if not exist "%PDCURSES_LIB%" (
    echo [ERROR] pdcurses library not found at %PDCURSES_LIB%
    exit /b 1
)

set "LINK_FLAGS=-static -static-libgcc -static-libstdc++"
if not "%CURL_LIB%"=="%CURL_LIB_A%" set "LINK_FLAGS="
if not "%YAMLCPP_LIB%"=="%YAMLCPP_LIB_A%" set "LINK_FLAGS="
if not "%PDCURSES_LIB%"=="%PDCURSES_LIB_A%" set "LINK_FLAGS="

rem ---------------------------------------------------------------------------
rem  Compile
rem ---------------------------------------------------------------------------
g++ -std=c++20 -Wall -Wextra -O2 %LINK_FLAGS% -DYAML_CPP_STATIC_DEFINE %CURL_STATIC_DEF% %INCLUDE_ARGS% %SRC_LIST% %SQLITE_OBJ% %LIB_ARGS% -o "%BUILD_DIR%\autogithubpullmerge.exe"

endlocal

