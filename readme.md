#autogithubpullmerge

A cross-platform tool to manage and monitor GitHub pull requests from a terminal user interface.

## Features
- Cross-platform build using CMake
- Linux, macOS and Windows install/build scripts
- Placeholder TUI application in C++23
- Dependencies managed with vcpkg
- Requires a curses development package for the TUI (`libncurses-dev` on
  Linux/macOS or `pdcurses` on Windows)
- Unit tests using Catch2
- SQLite-based history storage with CSV/JSON export
- Configurable logging with `--log-level` and optional `--log-file`
- Uses spdlog for colored console and rotating file logging
- Cross-platform compile scripts (MSVC on Windows, g++ on Linux/macOS) with
  C++23 support
- Dependencies verified to build with C++23 via vcpkg
- CLI options for GitHub API keys (`--api-key`, `--api-key-from-stream`,
  `--api-key-url`, `--api-key-url-user`, `--api-key-url-password`,
  `--api-key-file`)
- Rate limiting to control GitHub API usage
- Branch protection patterns to guard important branches
- Dry-run mode and HTTP/HTTPS proxy support

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
scripts\build_win.bat
```

The Windows build links against the static MSVC runtime to produce a
fully self-contained executable. Ensure all dependencies are installed
with the `x64-windows-static` vcpkg triplet to avoid `/MT` vs `/MD`
runtime mismatches.

The build script automatically locates `vswhere.exe` using the `VSWHERE`
environment variable, the `PATH`, standard Visual Studio Installer
directories or a Chocolatey installation before falling back to the default
location. It then calls `VsDevCmd.bat` so `cl.exe`
is available. Visual Studio Build Tools must be installed but the script can
be run from a normal command prompt.


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
bootstrapped and dependencies are installed. The Windows script mirrors the
logic in `build_win.bat` and automatically configures the MSVC environment.

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
`--log-file` writes messages to a rotating log file in addition to stdout.
Passing `--verbose` sets the logger to the `debug` level unless `--log-level`
specifies another level. Configuration files may also define `log_level`,
`log_pattern` and `log_file` values.

## Notifications

`GitHubPoller` can emit user notifications when pull requests are merged or
branches are purged. A basic `NotifySendNotifier` uses the `notify-send`
command on Linux desktops. Implementations derive from `agpm::Notifier` to
integrate with other systems. See `docs/notifications.md` for details.

## API Key Options

API keys can be provided in several ways:

- `--api-key` to specify a token directly (repeatable but not recommended)
- `--api-key-from-stream` to read tokens from standard input
- `--api-key-url` to fetch tokens from a remote URL with optional basic auth
- `--api-key-file` to load tokens from a JSON or YAML file
- if no other tokens are supplied, `GITHUB_TOKEN` (or `AGPM_API_KEY`) from the
  environment is used
- explicit CLI options, files, URLs or stdin tokens override the environment
  variable

## Polling Options

- `--poll-interval` sets how often the application polls GitHub for updates in
  seconds. A value of `0` disables polling.
- `--max-request-rate` limits the maximum number of GitHub requests per minute
  using a token bucket algorithm. When polling is enabled a background worker
  thread periodically invokes the GitHub API.
- `--pr-limit` limits how many pull requests to fetch when listing.
- `--pr-since` filters pull requests newer than the given duration
  (e.g. `30m`, `2h`, `1d`).
- `--sort` orders pull request titles by `alpha`, `reverse`, `alphanum` or
  `reverse-alphanum`.
- `--include-merged` lists merged pull requests in addition to open ones (off by default).
- `--only-poll-prs` polls only pull requests.
- `--only-poll-stray` enters an isolated stray-branch purge mode.
- `--reject-dirty` overrides protection and closes stray branches that have
  diverged.
- `--yes` assumes "yes" to confirmation prompts.
- `--purge-prefix` deletes branches with this prefix after their pull
  request is closed or merged, integrating cleanup into the merge workflow.
- `--auto-merge` merges pull requests automatically.

Destructive options (`--reject-dirty`, `--auto-merge`, `--purge-prefix`,
`--purge-only`) prompt for confirmation unless `--yes` is supplied.

## Networking

- `--http-timeout` sets the HTTP request timeout in seconds (default `30`).
- `--http-retries` sets how many times failed HTTP requests are retried (default `3`).
- `--download-limit` caps the download rate in bytes per second (0 = unlimited).
- `--upload-limit` caps the upload rate in bytes per second (0 = unlimited).
- `--max-download` limits total downloaded bytes (0 = unlimited).
- `--max-upload` limits total uploaded bytes (0 = unlimited).

These options may also be specified in JSON or YAML configuration files as
`http_timeout`, `http_retries`, `download_limit`, `upload_limit`,
`max_download` and `max_upload`.

## Rate Limiting

The `--max-request-rate` option throttles how many GitHub API calls are made
per minute to stay within rate limits.

```yaml
max_request_rate: 60
```

```json
{
  "max_request_rate": 60
}
```

## Branch Protection Patterns

Glob patterns supplied with `--protect-branch` define branches that must not be
modified. Patterns passed via `--protect-branch-exclude` remove protection for
matching names.

```yaml
protected_branches:
  - main
  - release/*
protected_branch_excludes:
  - release/temp/*
```

```json
{
  "protected_branches": ["main", "release/*"],
  "protected_branch_excludes": ["release/temp/*"]
}
```

## Dry-Run and Proxy Support

Use `--dry-run` to simulate operations without altering repositories. HTTP
requests may be routed through proxies using `--http-proxy` and
`--https-proxy`.

```bash
autogithubpullmerge --dry-run --http-proxy http://proxy --https-proxy http://secureproxy
```

```yaml
http_proxy: http://proxy
https_proxy: http://secureproxy
```

```json
{
  "http_proxy": "http://proxy",
  "https_proxy": "http://secureproxy"
}
```

## TUI Hotkeys

The terminal interface supports the following key bindings:

- `r` refreshes the pull request list.
- `m` merges the selected pull request.
- `q` quits the interface.
- Arrow keys navigate between pull requests.

## Examples

Example configuration files can be found in the `examples` directory:

- `config.yaml`
- `config.json`

You can combine configuration files with CLI flags:

```yaml
# config.yaml
http_timeout: 45
download_limit: 1048576
```

```bash
autogithubpullmerge --config config.yaml --http-retries 5 --upload-limit 512000
```

Or configure everything directly on the command line:

```bash
autogithubpullmerge --http-timeout 60 --http-retries 5 \
  --download-limit 1048576 --upload-limit 512000 \
  --max-download 10485760 --max-upload 5242880
```
