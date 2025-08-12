@echo off
setlocal EnableDelayedExpansion

echo -------------------
echo Compile script
echo -------------------


rem ---------------------------------------------------------------------------
rem  Determine project root
rem ---------------------------------------------------------------------------
echo.
echo Determining project root

for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
cd /d "%ROOT_DIR%" || exit /b 1

echo Root dir set to: %ROOT_DIR%

rem ---------------------------------------------------------------------------
rem  Prepare build directories
rem ---------------------------------------------------------------------------
echo.
echo Getting build dir.

set "BUILD_DIR=%ROOT_DIR%\build_gpp"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo Build dir set to: %BUILD_DIR%

rem ---------------------------------------------------------------------------
rem  Collect sources (add sqlite amalgamation manually)
rem ---------------------------------------------------------------------------
echo.
echo Getting source list.

set SRC_LIST=
for /r "%ROOT_DIR%\src" %%F in (*.cpp) do (
    set SRC_LIST=!SRC_LIST! "%%F"
)

echo Source list set to: %SRC_LIST%

echo.
echo Doing SQLite amalgamation.

set SQLITE_OBJ=
if exist "%ROOT_DIR%\libs\sqlite\sqlite3.c" (
    g++ -x c -O2 -I"%ROOT_DIR%\libs\sqlite" -c "%ROOT_DIR%\libs\sqlite\sqlite3.c" -o "%BUILD_DIR%\sqlite3.o" || exit /b 1
    set SQLITE_OBJ="%BUILD_DIR%\sqlite3.o"
)

echo Amalgamation done.

rem ---------------------------------------------------------------------------
rem  Include directories
rem ---------------------------------------------------------------------------
set "LIBS_DIR=%ROOT_DIR%\libs"

echo.
echo Include dir set to: %LIBS_DIR%

rem Dependency include and lib directories produced by their install steps
set "YAMLCPP_INC=%LIBS_DIR%\yaml-cpp\yaml-cpp_install\include"
set "YAMLCPP_LIB_A=%LIBS_DIR%\yaml-cpp\yaml-cpp_install\lib\libyaml-cpp.a"
set "YAMLCPP_LIB_DLL_A=%LIBS_DIR%\yaml-cpp\yaml-cpp_install\lib\libyaml-cpp.dll.a"
set "YAMLCPP_LIB_LIB=%LIBS_DIR%\yaml-cpp\yaml-cpp_install\lib\yaml-cpp.lib"
set "CURL_INC=%LIBS_DIR%\curl\curl_install\include"
set "CURL_LIB_A=%LIBS_DIR%\curl\curl_install\lib\libcurl.a"
set "CURL_LIB_DLL_A=%LIBS_DIR%\curl\curl_install\lib\libcurl.dll.a"
set "CURL_LIB_LIB=%LIBS_DIR%\curl\curl_install\lib\curl.lib"
set "NGHTTP2_INC=%LIBS_DIR%\nghttp2\nghttp2_install\include"
set "NGHTTP2_LIB_DIR=%LIBS_DIR%\nghttp2\nghttp2_install\lib"
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
if not exist "%NGHTTP2_INC%\nghttp2\nghttp2.h" (
    echo [ERROR] nghttp2 headers not found at %NGHTTP2_INC%
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
if not exist "%NGHTTP2_LIB_DIR%\libnghttp2.a" if not exist "%NGHTTP2_LIB_DIR%\libnghttp2.dll.a" if not exist "%NGHTTP2_LIB_DIR%\nghttp2.lib" (
    echo [ERROR] nghttp2 library not found at %NGHTTP2_LIB_DIR%
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
  -I"%NGHTTP2_INC%" ^
  -I"%LIBS_DIR%\sqlite"

echo Include args are: %INCLUDE_ARGS%

rem ---------------------------------------------------------------------------
rem  Library locations (expect static libs in these folders)
rem ---------------------------------------------------------------------------
echo.
echo Getting lib args.

rem Link against curl and its feature libraries statically
set "CURL_LIBS_DIR=%LIBS_DIR%\curl\curl_install\lib"
set "NGHTTP2_LIBS_DIR=%NGHTTP2_LIB_DIR%"

set LIB_ARGS=^
  -L"%CURL_LIBS_DIR%" ^
  -L"%NGHTTP2_LIBS_DIR%" ^
  -Wl,--start-group ^
    -lcurl -lssh2 -lnghttp2 ^
    -lngtcp2_crypto_quictls -lngtcp2 -lnghttp3 ^
    -lssl -lcrypto ^
    -lbrotlidec -lbrotlicommon -lzstd -lz ^
    -lidn2 -lunistring -lpsl -liconv ^
  -Wl,--end-group ^
  "%YAMLCPP_LIB%" "%PDCURSES_LIB%" ^
  -lws2_32 -lsecur32 -lcrypt32 -lwldap32 -lbcrypt -ladvapi32 -luser32 -lnormaliz -liphlpapi

echo Lib args set to: %LIB_ARGS%

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

echo Link flags set to: %LINK_FLAGS%

rem ---------------------------------------------------------------------------
rem  Compile
rem ---------------------------------------------------------------------------
echo.
echo Compilation going.
echo.

g++ -std=c++20 -Wall -Wextra -O2 %LINK_FLAGS% -DYAML_CPP_STATIC_DEFINE %CURL_STATIC_DEF% %INCLUDE_ARGS% %SRC_LIST% %SQLITE_OBJ% %LIB_ARGS% -o "%BUILD_DIR%\autogithubpullmerge.exe"

echo.
echo Compilation done.

endlocal

