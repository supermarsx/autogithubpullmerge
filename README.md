#autogithubpullmerge

A cross-platform tool to manage and monitor GitHub pull requests from a terminal user interface.

## Features
- Cross-platform build using CMake
- Linux, macOS and Windows install/build scripts
- Placeholder TUI application in C++20
- Includes ncurses/pdcurses fetched with `scripts/update_libs.sh`
- Requires an ncurses development package (e.g. `libncurses-dev`) for the TUI
- Unit tests using Catch2
- SQLite-based history storage with CSV/JSON export
- Configurable logging with `--log-level`
- Uses spdlog for colored console logging
- Cross-platform compile scripts using g++
- CLI options for GitHub API keys (`--api-key`, `--api-key-from-stream`,
  `--api-key-url`, `--api-key-url-user`, `--api-key-url-password`,
  `--api-key-file`)

## Building (Linux)
```bash
./scripts/install_linux.sh
./scripts/update_libs.sh
./scripts/build_linux.sh
```

## Building (macOS)
```bash
./scripts/install_mac.sh
./scripts/update_libs.sh
./scripts/build_mac.sh
```

## Building (Windows)
```powershell
./scripts/install_win.bat
./scripts/update_libs.bat
./scripts/build_win.bat
```

## Compiling with g++

On systems with g++ available, you can build the project without CMake using the
provided scripts:

```bash
./scripts/compile_linux.sh   # Linux
./scripts/compile_mac.sh     # macOS
./scripts/compile_win.bat    # Windows
```
Run the matching `install_*` script for your platform first to install system
packages like **libcurl**, **sqlite3** and **ncurses**. Then use `update_libs.sh` (or
`update_libs.bat` on Windows) to populate the `libs` directory with clones of
**CLI11**, **yaml-cpp**, **libyaml**, **nlohmann/json**, **spdlog**, **curl**,
the SQLite amalgamation and **PDCurses** before compiling. The Windows compile
script links the application statically so the resulting binary has no runtime
library dependencies.

## Generating Documentation

The project uses Doxygen for API documentation. Run the following from the
repository root to generate docs in `docs/build`:

```bash
doxygen docs/Doxyfile
```

## Logging

Logging output uses the **spdlog** library with colorized messages by default.
The `--log-level` option controls verbosity. Valid levels include
`trace`, `debug`, `info`, `warn`, `error`, `critical` and `off`.
Passing `--verbose` sets the logger to the `debug` level unless `--log-level`
specifies another level.

## API Key Options

API keys can be provided in several ways:

- `--api-key` to specify a token directly (repeatable but not recommended)
- `--api-key-from-stream` to read tokens from standard input
- `--api-key-url` to fetch tokens from a remote URL with optional basic auth
- `--api-key-file` to load tokens from a JSON or YAML file

## Polling Options

- `--poll-interval` sets how often the application polls GitHub for updates in
  seconds. A value of `0` disables polling.
- `--max-request-rate` limits the maximum number of GitHub requests per minute
  using a token bucket algorithm. When polling is enabled a background worker
  thread periodically invokes the GitHub API.

## Examples

Example configuration files can be found in the `examples` directory:

- `config.yaml`
- `config.json`
