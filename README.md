#autogithubpullmerge

A cross-platform tool to manage and monitor GitHub pull requests from a terminal user interface.

## Features
- Cross-platform build using CMake
- Linux, macOS and Windows install/build scripts
- Placeholder TUI application in C++20
- Dependencies managed with vcpkg
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
cmake --preset vcpkg
cmake --build --preset vcpkg
```

## Building (macOS)
```bash
./scripts/install_mac.sh
cmake --preset vcpkg
cmake --build --preset vcpkg
```

## Building (Windows)
```powershell
./scripts/install_win.bat
cmake --preset vcpkg
cmake --build --preset vcpkg --config Release
```

The install scripts clone and bootstrap [vcpkg](https://github.com/microsoft/vcpkg)
if `VCPKG_ROOT` is not set, then install all dependencies declared in
`vcpkg.json`. The `vcpkg` CMake preset points to the repository's bundled
`vcpkg` directory, so the build uses the toolchain without extra flags. To use a
different vcpkg installation, edit `CMakeUserPresets.json` or export
`VCPKG_ROOT` and add it to your `PATH`.

## Compiling with g++

The `compile_*` scripts wrap the CMake preset for convenience:

```bash
./scripts/compile_linux.sh   # Linux
./scripts/compile_mac.sh     # macOS
./scripts/compile_win.bat    # Windows
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
setx VCPKG_DEFAULT_TRIPLET x64-windows /M
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
cmake --preset vcpkg
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
- `--poll-prs` polls only pull requests.
- `--poll-stray-branches` polls only stray branches.
- `--auto-reject-dirty` closes stray branches that have diverged.
- `--purge-branch-prefix` deletes branches with this prefix after their pull
  request is closed or merged.
- `--auto-merge` merges pull requests automatically.

## Examples

Example configuration files can be found in the `examples` directory:

- `config.yaml`
- `config.json`
