#autogithubpullmerge

A cross-platform tool to manage and monitor GitHub pull requests from a terminal user interface.

## Features
- Cross-platform build using CMake
- Linux, macOS and Windows install/build scripts
- Placeholder TUI application in C++20
- Dependencies managed with vcpkg
- Requires a curses development package for the TUI (`libncurses-dev` on
  Linux/macOS or `pdcurses` on Windows)
- Unit tests using Catch2
- SQLite-based history storage with CSV/JSON export
- Configurable logging with `--log-level`
- Uses spdlog for colored console logging
- Cross-platform compile scripts (MSVC on Windows, g++ on Linux/macOS)
- CLI options for GitHub API keys (`--api-key`, `--api-key-from-stream`,
  `--api-key-url`, `--api-key-url-user`, `--api-key-url-password`,
  `--api-key-file`)

## Building (Linux)
```bash
./scripts/install_linux.sh
cmake --preset vcpkg --fresh
cmake --build --preset vcpkg
```

## Building (macOS)
```bash
./scripts/install_mac.sh
cmake --preset vcpkg --fresh
cmake --build --preset vcpkg
```

## Building (Windows)
```bat
scripts\install_win.bat
cmake --preset vcpkg --fresh
cmake --build --preset vcpkg
```


The install scripts clone and bootstrap
[vcpkg](https://github.com/microsoft/vcpkg) if `VCPKG_ROOT` is not set, then
install all dependencies declared in `vcpkg.json`. Ninja is installed so the
Windows and Unix builds work out of the box. On Linux and macOS the `vcpkg`
CMake preset automatically locates the toolchain. To use a different vcpkg
installation, create or edit `CMakeUserPresets.json` or export `VCPKG_ROOT` and
add it to your `PATH`.

## Compiling

The `compile_*` scripts wrap the platform build commands:

```bash
./scripts/compile_linux.sh   # Linux (g++)
./scripts/compile_mac.sh     # macOS (g++)
./scripts/compile_win.bat    # Windows (MSVC)
```
Run the matching `install_*` script for your platform first to ensure vcpkg is
bootstrapped and dependencies are installed.

## vcpkg Manual Setup

To install vcpkg manually without the helper scripts:

### Windows
```powershell
git clone https://github.com/microsoft/vcpkg %USERPROFILE%\vcpkg
%USERPROFILE%\vcpkg\bootstrap-vcpkg.bat
%USERPROFILE%\vcpkg\vcpkg.exe integrate install
setx VCPKG_DEFAULT_TRIPLET x64-windows-static /M
setx VCPKG_ROOT "%USERPROFILE%\vcpkg" /M
setx PATH "%PATH%;%VCPKG_ROOT%" /M
```

### Linux
```bash
git clone https://github.com/microsoft/vcpkg ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
echo "export VCPKG_ROOT=\"$HOME/vcpkg\"" >> ~/.bashrc
echo "export PATH=\"\$VCPKG_ROOT:\$PATH\"" >> ~/.bashrc
```

### macOS
```bash
git clone https://github.com/microsoft/vcpkg ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
echo "export VCPKG_ROOT=\"$HOME/vcpkg\"" >> ~/.zprofile
echo "export PATH=\"\$VCPKG_ROOT:\$PATH\"" >> ~/.zprofile
```

After setting up vcpkg, configure and build with the `vcpkg` CMake preset:

```bash
cmake --preset vcpkg --fresh
cmake --build --preset vcpkg
```

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
- `--pr-limit` limits how many pull requests to fetch when listing.
- `--pr-since` filters pull requests newer than the given duration
  (e.g. `30m`, `2h`, `1d`).
- `--include-merged` lists merged pull requests in addition to open ones (off by default).
- `--only-poll-prs` polls only pull requests.
- `--only-poll-stray` enters an isolated stray-branch purge mode.
- `--reject-dirty` overrides protection and closes stray branches that have
  diverged.
- `--purge-prefix` deletes branches with this prefix after their pull
  request is closed or merged, integrating cleanup into the merge workflow.
- `--auto-merge` merges pull requests automatically.

## Examples

Example configuration files can be found in the `examples` directory:

- `config.yaml`
- `config.json`
