@echo off
setlocal EnableDelayedExpansion

echo.
echo -------------
echo updating libs
echo -------------

echo.
echo Getting libs dir.

rem Determine libs directory
for %%I in ("%~dp0..") do set "BASE_DIR=%%~fI"
set "LIBS_DIR=!BASE_DIR!\libs"
if not exist "!LIBS_DIR!" mkdir "!LIBS_DIR!"

echo Libs dir set to: %LIBS_DIR%

echo.
echo Checking repos for updates.

call :clone_or_update https://github.com/CLIUtils/CLI11.git CLI11
call :clone_or_update https://github.com/jbeder/yaml-cpp.git yaml-cpp
call :clone_or_update https://github.com/yaml/libyaml.git libyaml
call :clone_or_update https://github.com/nlohmann/json.git json
call :clone_or_update https://github.com/gabime/spdlog.git spdlog
call :clone_or_update https://github.com/nghttp2/nghttp2.git nghttp2
call :clone_or_update https://github.com/wmcbrine/PDCurses.git pdcurses

echo Repos checked and done.

rem Build and install yaml-cpp into a local install directory
echo.
echo Build and install yaml cpp

set "YAMLCPP_SRC=!LIBS_DIR!\yaml-cpp"
set "YAMLCPP_INSTALL=!YAMLCPP_SRC!\yaml-cpp_install"
set "YAMLCPP_LIB_A=!YAMLCPP_INSTALL!\lib\libyaml-cpp.a"
set "YAMLCPP_LIB_LIB=!YAMLCPP_INSTALL!\lib\yaml-cpp.lib"

del !YAMLCPP_SRC!\build\CMakeCache.txt
cmake -S "!YAMLCPP_SRC!" -B "!YAMLCPP_SRC!\build" -G "MinGW Makefiles" -DBUILD_SHARED_LIBS=OFF -DYAML_CPP_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX="!YAMLCPP_INSTALL!"
cmake --build "!YAMLCPP_SRC!\build" --config Release || goto :eof
cmake --install "!YAMLCPP_SRC!\build" || goto :eof

echo Done yaml cpp.

rem Build and install nghttp2 static library
echo.
echo Build and install nghttp2

set "NGHTTP2_SRC=!LIBS_DIR!\nghttp2"
set "NGHTTP2_INSTALL=!NGHTTP2_SRC!\nghttp2_install"
set "NGHTTP2_LIB_A=!NGHTTP2_INSTALL!\lib\libnghttp2.a"
set "NGHTTP2_LIB_LIB=!NGHTTP2_INSTALL!\lib\nghttp2.lib"

del !NGHTTP2_SRC!\build\CMakeCache.txt 2>nul
if not exist "!NGHTTP2_LIB_A!" if not exist "!NGHTTP2_LIB_LIB!" (
    cmake -S "!NGHTTP2_SRC!" -B "!NGHTTP2_SRC!\build" -G "MinGW Makefiles" -DBUILD_SHARED_LIBS=OFF -DENABLE_EXAMPLES=OFF -DENABLE_HPACK_TOOLS=OFF -DENABLE_ASIO_LIB=OFF -DCMAKE_INSTALL_PREFIX="!NGHTTP2_INSTALL!"
    cmake --build "!NGHTTP2_SRC!\build" --config Release || goto :eof
    cmake --install "!NGHTTP2_SRC!\build" || goto :eof
)

echo Done nghttp2.

rem Fetch prebuilt curl for Windows to obtain libcurl.a
echo.
echo Getting curl amalgamation.

set CURL_VER=8.15.0_4
set CURL_ZIP=curl-%CURL_VER%-win64-mingw.zip
set CURL_URL=https://curl.se/windows/dl-%CURL_VER%/%CURL_ZIP%
set CURL_DIR=!LIBS_DIR!\curl
set CURL_INSTALL=!CURL_DIR!\curl_install
if not exist "!CURL_INSTALL!" mkdir "!CURL_INSTALL!"
    curl -L %CURL_URL% -o "!CURL_DIR!\%CURL_ZIP%" || goto :eof
    powershell -Command "Expand-Archive -Path '!CURL_DIR!\%CURL_ZIP%' -DestinationPath '!CURL_DIR!' -Force" || goto :eof
    powershell -Command "Copy-Item -Path '!CURL_DIR!\curl-%CURL_VER%-win64-mingw\include' -Destination '!CURL_INSTALL!' -Recurse -Force" || goto :eof
    powershell -Command "Copy-Item -Path '!CURL_DIR!\curl-%CURL_VER%-win64-mingw\lib' -Destination '!CURL_INSTALL!' -Recurse -Force" || goto :eof
    rmdir /s /q "!CURL_DIR!\curl-%CURL_VER%-win64-mingw"
    del "!CURL_DIR!\%CURL_ZIP%"

echo Got curl amalgamation.

echo.
echo Getting sqlite amalgamation.

set SQLITE_VER=3430000
set SQLITE_YEAR=2023
set SQLITE_ZIP=sqlite-amalgamation-%SQLITE_VER%.zip
set SQLITE_DIR=!LIBS_DIR!\sqlite
if not exist "!SQLITE_DIR!" mkdir "!SQLITE_DIR!"
    curl -L https://sqlite.org/%SQLITE_YEAR%/%SQLITE_ZIP% -o "!SQLITE_DIR!\%SQLITE_ZIP%" || goto :eof
    powershell -Command "Expand-Archive -Path '!SQLITE_DIR!\%SQLITE_ZIP%' -DestinationPath '!SQLITE_DIR!' -Force" || goto :eof
    move "!SQLITE_DIR!\sqlite-amalgamation-%SQLITE_VER%\*" "!SQLITE_DIR!" >nul
    rmdir /s /q "!SQLITE_DIR!\sqlite-amalgamation-%SQLITE_VER%"
    del "!SQLITE_DIR!\%SQLITE_ZIP%"

echo Got sqlite amalgamation.

echo.
echo 

rem Build and install pdcurses static library
set "PDC_SRC=!LIBS_DIR!\pdcurses"
set "PDC_INSTALL=!PDC_SRC!\pdcurses_install"

    where mingw32-make >nul 2>&1
    if not errorlevel 1 (
        rem use mingw32-make if available
        pushd "!PDC_SRC!\wincon" || goto :eof
        mingw32-make -f Makefile || (popd & goto :eof)
        set "PDC_LIB=!PDC_SRC!\wincon\pdcurses.a"
        popd
    ) else (
        where make >nul 2>&1
        if not errorlevel 1 (
            rem fallback to make
            pushd "!PDC_SRC!\wincon" || goto :eof
            make -f Makefile || (popd & goto :eof)
            set "PDC_LIB=!PDC_SRC!\wincon\pdcurses.a"
            popd
        ) else (
            rem last resort, try nmake with Visual C++ makefile
            where nmake >nul 2>&1 || goto :eof
            pushd "!PDC_SRC!\wincon" || goto :eof
            nmake -f Makefile.vc || (popd & goto :eof)
            set "PDC_LIB=!PDC_SRC!\wincon\pdcurses.lib"
            popd
        )
    )
    mkdir "!PDC_INSTALL!\include" 2>nul
    mkdir "!PDC_INSTALL!\lib" 2>nul
    copy "!PDC_SRC!\curses.h" "!PDC_INSTALL!\include\" >nul || goto :eof
    copy "!PDC_LIB!" "!PDC_INSTALL!\lib\" >nul || goto :eof
)


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
