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

## Building (Linux)
```bash
./scripts/install_linux.sh
./scripts/build_linux.sh
```

## Building (macOS)
```bash
./scripts/install_mac.sh
./scripts/build_mac.sh
```

If Homebrew is installed in a custom location, set the `PKG_CONFIG` environment
variable to the full path of `pkg-config` before running the build scripts:

```bash
export PKG_CONFIG=$(which pkg-config)
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
