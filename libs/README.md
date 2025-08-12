This directory holds third-party dependencies used by autogithubpullmerge.
Run `./scripts/update_libs.sh` or `./scripts/update_libs.bat` to clone or update
third-party projects such as **CLI11**, **yaml-cpp**, **libyaml**, **nlohmann/json**,
**spdlog**, **curl**, **nghttp2**, **sqlite**, **ncurses**, **libev**, **c-ares**,
**zlib**, **brotli**, **openssl**, **ngtcp2**, **nghttp3**, **systemd**,
**jansson**, **libevent**, **libxml2** and **jemalloc**. After cloning, the SQLite
repository's `VERSION` file is renamed to `VERSION.txt` to prevent conflicts
with the C++ `<version>` header. The contents of this folder are ignored by
git so clones are not committed to the repository.
