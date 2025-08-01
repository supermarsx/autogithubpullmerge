@echo off
setlocal EnableDelayedExpansion

rem Determine root directory
for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
cd /d "%ROOT_DIR%"

set "BUILD_DIR=%ROOT_DIR%\build_gpp"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

set "SRC_LIST="
for /r "%ROOT_DIR%\src" %%F in (*.cpp) do set "SRC_LIST=!SRC_LIST! \"%%F\""

set "INCLUDE=%ROOT_DIR%\include"
set "LIBS_DIR=%ROOT_DIR%\libs"
set "INCLUDE_ARGS=-I"%INCLUDE%" -I"%LIBS_DIR%\CLI11\include" -I"%LIBS_DIR%\json\include" -I"%LIBS_DIR%\spdlog\include" -I"%LIBS_DIR%\yaml-cpp\include" -I"%LIBS_DIR%\libyaml\include" -I"%LIBS_DIR%\pdcurses" -I"%LIBS_DIR%\curl\include" -I"%LIBS_DIR%\sqlite""
set "LIB_ARGS=-L"%LIBS_DIR%\curl\lib" -L"%LIBS_DIR%\sqlite" -L"%LIBS_DIR%\yaml-cpp\lib" -L"%LIBS_DIR%\libyaml\lib" -L"%LIBS_DIR%\pdcurses\lib""

g++ -std=c++20 -Wall -Wextra -O2 -static -static-libgcc -static-libstdc++ -DYAML_CPP_STATIC_DEFINE %INCLUDE_ARGS% %SRC_LIST% %LIB_ARGS% -lcurl -lsqlite3 -lyaml-cpp -lyaml -lpdcurses -o "%BUILD_DIR%\autogithubpullmerge.exe"

endlocal
