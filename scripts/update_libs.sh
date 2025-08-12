#!/usr/bin/env bash
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LIBS_DIR="${SCRIPT_DIR}/../libs"
mkdir -p "$LIBS_DIR"

clone_or_update() {
	local repo="$1"
	local dir="$LIBS_DIR/$2"
	if [ -d "$dir/.git" ]; then
		git -C "$dir" pull --ff-only
	else
		git clone --depth 1 "$repo" "$dir"
	fi
}

clone_or_update https://github.com/CLIUtils/CLI11.git CLI11
clone_or_update https://github.com/jbeder/yaml-cpp.git yaml-cpp
clone_or_update https://github.com/yaml/libyaml.git libyaml
clone_or_update https://github.com/nlohmann/json.git json
clone_or_update https://github.com/gabime/spdlog.git spdlog
clone_or_update https://github.com/curl/curl.git curl
clone_or_update https://github.com/nghttp2/nghttp2.git nghttp2
clone_or_update https://github.com/c-ares/c-ares.git c-ares
clone_or_update https://github.com/madler/zlib.git zlib
clone_or_update https://github.com/google/brotli.git brotli
clone_or_update https://github.com/openssl/openssl.git openssl
clone_or_update https://github.com/ngtcp2/ngtcp2.git ngtcp2
clone_or_update https://github.com/ngtcp2/nghttp3.git nghttp3
clone_or_update https://github.com/enki/libev.git libev
clone_or_update https://github.com/akheron/jansson.git jansson
clone_or_update https://github.com/libevent/libevent.git libevent
clone_or_update https://github.com/GNOME/libxml2.git libxml2
clone_or_update https://github.com/jemalloc/jemalloc.git jemalloc
clone_or_update https://github.com/systemd/systemd.git systemd

# Build and install yaml-cpp into a local install directory
YAMLCPP_SRC="$LIBS_DIR/yaml-cpp"
YAMLCPP_INSTALL="$YAMLCPP_SRC/yaml-cpp_install"
if [ ! -f "$YAMLCPP_INSTALL/lib/libyaml-cpp.a" ]; then
    cmake -S "$YAMLCPP_SRC" -B "$YAMLCPP_SRC/build" -DBUILD_SHARED_LIBS=OFF -DYAML_CPP_BUILD_TESTS=OFF \
        -DCMAKE_INSTALL_PREFIX="$YAMLCPP_INSTALL"
    cmake --build "$YAMLCPP_SRC/build" --config Release
    cmake --install "$YAMLCPP_SRC/build"
fi

# Build and install zlib
ZLIB_SRC="$LIBS_DIR/zlib"
ZLIB_INSTALL="$ZLIB_SRC/zlib_install"
if [ ! -f "$ZLIB_INSTALL/lib/libz.a" ]; then
    cmake -S "$ZLIB_SRC" -B "$ZLIB_SRC/build" -DBUILD_SHARED_LIBS=OFF \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX="$ZLIB_INSTALL"
    cmake --build "$ZLIB_SRC/build" --config Release
    cmake --install "$ZLIB_SRC/build"
fi

# Build and install Brotli
BROTLI_SRC="$LIBS_DIR/brotli"
BROTLI_INSTALL="$BROTLI_SRC/brotli_install"
if [ ! -f "$BROTLI_INSTALL/lib/libbrotlienc.a" ]; then
    cmake -S "$BROTLI_SRC" -B "$BROTLI_SRC/build" -DBUILD_SHARED_LIBS=OFF \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX="$BROTLI_INSTALL"
    cmake --build "$BROTLI_SRC/build" --config Release
    cmake --install "$BROTLI_SRC/build"
fi

# Build and install OpenSSL
OPENSSL_SRC="$LIBS_DIR/openssl"
OPENSSL_INSTALL="$OPENSSL_SRC/openssl_install"
if [ ! -f "$OPENSSL_INSTALL/lib/libssl.a" ]; then
    (cd "$OPENSSL_SRC" && ./Configure no-shared --prefix="$OPENSSL_INSTALL" && make -j"$(nproc)" && make install_sw)
fi

# Build and install c-ares
CARES_SRC="$LIBS_DIR/c-ares"
CARES_INSTALL="$CARES_SRC/cares_install"
if [ ! -f "$CARES_INSTALL/lib/libcares.a" ]; then
    cmake -S "$CARES_SRC" -B "$CARES_SRC/build" -DBUILD_SHARED_LIBS=OFF \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX="$CARES_INSTALL"
    cmake --build "$CARES_SRC/build" --config Release
    cmake --install "$CARES_SRC/build"
fi

# Build and install libev
LIBEV_SRC="$LIBS_DIR/libev"
LIBEV_INSTALL="$LIBEV_SRC/libev_install"
if [ ! -f "$LIBEV_INSTALL/lib/libev.a" ]; then
    (cd "$LIBEV_SRC" && ./configure --disable-shared --prefix="$LIBEV_INSTALL" && make -j"$(nproc)" && make install)
fi

# Build and install jansson
JANSSON_SRC="$LIBS_DIR/jansson"
JANSSON_INSTALL="$JANSSON_SRC/jansson_install"
if [ ! -f "$JANSSON_INSTALL/lib/libjansson.a" ]; then
    cmake -S "$JANSSON_SRC" -B "$JANSSON_SRC/build" -DJANSSON_BUILD_SHARED_LIBS=OFF \
        -DCMAKE_INSTALL_PREFIX="$JANSSON_INSTALL"
    cmake --build "$JANSSON_SRC/build" --config Release
    cmake --install "$JANSSON_SRC/build"
fi

# Build and install systemd
SYSTEMD_SRC="$LIBS_DIR/systemd"
SYSTEMD_INSTALL="$SYSTEMD_SRC/systemd_install"
if [ ! -f "$SYSTEMD_INSTALL/lib/libsystemd.a" ]; then
    (cd "$SYSTEMD_SRC" && meson setup build --prefix="$SYSTEMD_INSTALL" -Dstatic=true && meson compile -C build && meson install -C build)
fi

# Build and install libevent
LIBEVENT_SRC="$LIBS_DIR/libevent"
LIBEVENT_INSTALL="$LIBEVENT_SRC/libevent_install"
if [ ! -f "$LIBEVENT_INSTALL/lib/libevent_core.a" ]; then
    cmake -S "$LIBEVENT_SRC" -B "$LIBEVENT_SRC/build" -DBUILD_SHARED_LIBS=OFF \
        -DEVENT__DISABLE_OPENSSL=OFF -DCMAKE_INSTALL_PREFIX="$LIBEVENT_INSTALL"
    cmake --build "$LIBEVENT_SRC/build" --config Release
    cmake --install "$LIBEVENT_SRC/build"
fi

# Build and install libxml2
LIBXML2_SRC="$LIBS_DIR/libxml2"
LIBXML2_INSTALL="$LIBXML2_SRC/libxml2_install"
if [ ! -f "$LIBXML2_INSTALL/lib/libxml2.a" ]; then
    cmake -S "$LIBXML2_SRC" -B "$LIBXML2_SRC/build" -DBUILD_SHARED_LIBS=OFF \
        -DLIBXML2_WITH_ICONV=OFF -DLIBXML2_WITH_PYTHON=OFF -DCMAKE_INSTALL_PREFIX="$LIBXML2_INSTALL"
    cmake --build "$LIBXML2_SRC/build" --config Release
    cmake --install "$LIBXML2_SRC/build"
fi

# Build and install jemalloc
JEMALLOC_SRC="$LIBS_DIR/jemalloc"
JEMALLOC_INSTALL="$JEMALLOC_SRC/jemalloc_install"
if [ ! -f "$JEMALLOC_INSTALL/lib/libjemalloc.a" ]; then
    (cd "$JEMALLOC_SRC" && ./autogen.sh && ./configure --disable-shared --prefix="$JEMALLOC_INSTALL" && make -j"$(nproc)" && make install)
fi

# Build and install nghttp3
NGHTTP3_SRC="$LIBS_DIR/nghttp3"
NGHTTP3_INSTALL="$NGHTTP3_SRC/nghttp3_install"
if [ ! -f "$NGHTTP3_INSTALL/lib/libnghttp3.a" ]; then
    cmake -S "$NGHTTP3_SRC" -B "$NGHTTP3_SRC/build" -DBUILD_SHARED_LIBS=OFF \
        -DCMAKE_INSTALL_PREFIX="$NGHTTP3_INSTALL"
    cmake --build "$NGHTTP3_SRC/build" --config Release
    cmake --install "$NGHTTP3_SRC/build"
fi

# Build and install ngtcp2
NGTCP2_SRC="$LIBS_DIR/ngtcp2"
NGTCP2_INSTALL="$NGTCP2_SRC/ngtcp2_install"
if [ ! -f "$NGTCP2_INSTALL/lib/libngtcp2.a" ]; then
    cmake -S "$NGTCP2_SRC" -B "$NGTCP2_SRC/build" -DBUILD_SHARED_LIBS=OFF \
        -DOPENSSL_ROOT_DIR="$OPENSSL_INSTALL" \
        -DNGHTTP3_INCLUDE_DIR="$NGHTTP3_INSTALL/include" \
        -DNGHTTP3_LIBRARY="$NGHTTP3_INSTALL/lib/libnghttp3.a" \
        -DCMAKE_INSTALL_PREFIX="$NGTCP2_INSTALL"
    cmake --build "$NGTCP2_SRC/build" --config Release
    cmake --install "$NGTCP2_SRC/build"
fi

# Make locally installed libraries discoverable for subsequent builds
export PKG_CONFIG_PATH="$ZLIB_INSTALL/lib/pkgconfig:$BROTLI_INSTALL/lib/pkgconfig:$CARES_INSTALL/lib/pkgconfig:$LIBEV_INSTALL/lib/pkgconfig:$JANSSON_INSTALL/lib/pkgconfig:$SYSTEMD_INSTALL/lib/pkgconfig:$LIBEVENT_INSTALL/lib/pkgconfig:$LIBXML2_INSTALL/lib/pkgconfig:$JEMALLOC_INSTALL/lib/pkgconfig:$NGHTTP3_INSTALL/lib/pkgconfig:$NGTCP2_INSTALL/lib/pkgconfig:${PKG_CONFIG_PATH}"
export CMAKE_PREFIX_PATH="$OPENSSL_INSTALL;$ZLIB_INSTALL;$BROTLI_INSTALL;$CARES_INSTALL;$LIBEV_INSTALL;$JANSSON_INSTALL;$SYSTEMD_INSTALL;$LIBEVENT_INSTALL;$LIBXML2_INSTALL;$JEMALLOC_INSTALL;$NGHTTP3_INSTALL;$NGTCP2_INSTALL;$CMAKE_PREFIX_PATH"

# Build and install nghttp2 into a local install directory
NGHTTP2_SRC="$LIBS_DIR/nghttp2"
NGHTTP2_INSTALL="$NGHTTP2_SRC/nghttp2_install"
if [ ! -f "$NGHTTP2_INSTALL/lib/libnghttp2.a" ]; then
    cmake -S "$NGHTTP2_SRC" -B "$NGHTTP2_SRC/build" -DBUILD_SHARED_LIBS=OFF \
        -DENABLE_EXAMPLES=OFF -DENABLE_HPACK_TOOLS=OFF -DENABLE_ASIO_LIB=OFF \
        -DZLIB_INCLUDE_DIR="$ZLIB_INSTALL/include" -DZLIB_LIBRARY="$ZLIB_INSTALL/lib/libz.a" \
        -DLIBCARES_LIBRARY="$CARES_INSTALL/lib/libcares.a" -DLIBCARES_INCLUDE_DIR="$CARES_INSTALL/include" \
        -DLIBEV_LIBRARY="$LIBEV_INSTALL/lib/libev.a" -DLIBEV_INCLUDE_DIR="$LIBEV_INSTALL/include" \
        -DOPENSSL_ROOT_DIR="$OPENSSL_INSTALL" \
        -DLIBBROTLIENC_LIBRARY="$BROTLI_INSTALL/lib/libbrotlienc.a" \
        -DLIBBROTLICOMMON_LIBRARY="$BROTLI_INSTALL/lib/libbrotlicommon.a" \
        -DLIBBROTLIDEC_LIBRARY="$BROTLI_INSTALL/lib/libbrotlidec.a" \
        -DNGTCP2_LIBRARY="$NGTCP2_INSTALL/lib/libngtcp2.a" -DNGTCP2_INCLUDE_DIR="$NGTCP2_INSTALL/include" \
        -DNGHTTP3_LIBRARY="$NGHTTP3_INSTALL/lib/libnghttp3.a" -DNGHTTP3_INCLUDE_DIR="$NGHTTP3_INSTALL/include" \
        -DJANSSON_LIBRARY="$JANSSON_INSTALL/lib/libjansson.a" -DJANSSON_INCLUDE_DIR="$JANSSON_INSTALL/include" \
        -DSYSTEMD_LIBRARIES="$SYSTEMD_INSTALL/lib/libsystemd.a" -DSYSTEMD_INCLUDE_DIRS="$SYSTEMD_INSTALL/include" \
        -DLIBEVENT_CORE_LIBRARY="$LIBEVENT_INSTALL/lib/libevent_core.a" \
        -DLIBEVENT_OPENSSL_LIBRARY="$LIBEVENT_INSTALL/lib/libevent_openssl.a" \
        -DLIBEVENT_EXTRA_LIBRARY="$LIBEVENT_INSTALL/lib/libevent_extra.a" \
        -DLIBXML2_LIBRARY="$LIBXML2_INSTALL/lib/libxml2.a" -DLIBXML2_INCLUDE_DIR="$LIBXML2_INSTALL/include/libxml2" \
        -DJEMALLOC_LIBRARY="$JEMALLOC_INSTALL/lib/libjemalloc.a" -DJEMALLOC_INCLUDE_DIR="$JEMALLOC_INSTALL/include" \
        -DCMAKE_INSTALL_PREFIX="$NGHTTP2_INSTALL"
    cmake --build "$NGHTTP2_SRC/build" --config Release
    cmake --install "$NGHTTP2_SRC/build"
fi

# Build and install curl into a local install directory
CURL_SRC="$LIBS_DIR/curl"
CURL_INSTALL="$CURL_SRC/curl_install"
if [ ! -f "$CURL_INSTALL/lib/libcurl.a" ]; then
    cmake -S "$CURL_SRC" -B "$CURL_SRC/build" -DBUILD_SHARED_LIBS=OFF -DBUILD_CURL_EXE=OFF \
        -DCMAKE_INSTALL_PREFIX="$CURL_INSTALL" \
        -DNGHTTP2_INCLUDE_DIR="$NGHTTP2_INSTALL/include" \
        -DNGHTTP2_LIBRARY="$NGHTTP2_INSTALL/lib/libnghttp2.a" \
        -DNGHTTP3_INCLUDE_DIR="$NGHTTP3_INSTALL/include" \
        -DNGHTTP3_LIBRARY="$NGHTTP3_INSTALL/lib/libnghttp3.a" \
        -DNGTCP2_INCLUDE_DIR="$NGTCP2_INSTALL/include" \
        -DNGTCP2_LIBRARY="$NGTCP2_INSTALL/lib/libngtcp2.a" \
        -DOPENSSL_ROOT_DIR="$OPENSSL_INSTALL" \
        -DZLIB_LIBRARY="$ZLIB_INSTALL/lib/libz.a" -DZLIB_INCLUDE_DIR="$ZLIB_INSTALL/include" \
        -DLIBBROTLIENC_LIBRARY="$BROTLI_INSTALL/lib/libbrotlienc.a" \
        -DLIBBROTLICOMMON_LIBRARY="$BROTLI_INSTALL/lib/libbrotlicommon.a" \
        -DLIBBROTLIDEC_LIBRARY="$BROTLI_INSTALL/lib/libbrotlidec.a"
    cmake --build "$CURL_SRC/build" --config Release
    cmake --install "$CURL_SRC/build"
fi

# Download SQLite amalgamation containing sqlite3.c and sqlite3.h
SQLITE_VER=3430000
SQLITE_YEAR=2023
SQLITE_ZIP="sqlite-amalgamation-${SQLITE_VER}.zip"
SQLITE_DIR="$LIBS_DIR/sqlite"
mkdir -p "$SQLITE_DIR"
if [ ! -f "$SQLITE_DIR/sqlite3.h" ]; then
    curl -L "https://sqlite.org/${SQLITE_YEAR}/${SQLITE_ZIP}" -o "$SQLITE_DIR/${SQLITE_ZIP}"
    unzip -oq "$SQLITE_DIR/${SQLITE_ZIP}" -d "$SQLITE_DIR"
    mv "$SQLITE_DIR/sqlite-amalgamation-${SQLITE_VER}"/* "$SQLITE_DIR"
    rmdir "$SQLITE_DIR/sqlite-amalgamation-${SQLITE_VER}"
    rm "$SQLITE_DIR/${SQLITE_ZIP}"
fi

clone_or_update https://github.com/wmcbrine/PDCurses.git pdcurses
