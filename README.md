#autogithubpullmerge

A cross-platform tool to manage and monitor GitHub pull requests from a terminal user interface.

## Features
- Cross-platform build using CMake
- Linux, macOS and Windows install/build scripts
- Placeholder TUI application in C++20
- Unit tests using Catch2
- SQLite-based history storage with CSV/JSON export
- Configurable logging with `--log-level`
- Cross-platform compile scripts using g++
- CLI options for GitHub API keys (`--api-key`, `--api-key-from-stream`,
  `--api-key-url`, `--api-key-url-user`, `--api-key-url-password`,
  `--api-key-file`)

## Building (Linux)
```bash
./scripts/install_linux.sh
./scripts/build_linux.sh
```

## Compiling with g++

On systems with g++ available, you can build the project without CMake using the
provided scripts:

```bash
./scripts/compile_linux.sh   # Linux
./scripts/compile_mac.sh     # macOS
./scripts/compile_win.ps1    # Windows
```

## Generating Documentation

The project uses Doxygen for API documentation. Run the following from the
repository root to generate docs in `docs/build`:

```bash
doxygen docs/Doxyfile
```

## Logging

Use the `--log-level` option to control verbosity. Valid levels include
`trace`, `debug`, `info`, `warn`, `error`, `critical` and `off`.

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
  using a token bucket algorithm.
