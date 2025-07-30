# autogithubpullmerge

A cross-platform tool to manage and monitor GitHub pull requests from a terminal user interface.

## Features
- Cross-platform build using CMake
- Linux, macOS and Windows install/build scripts
- Placeholder TUI application in C++20
- Unit tests using Catch2

## Building (Linux)
```bash
./scripts/install_linux.sh
./scripts/build_linux.sh
```

## Generating Documentation

The project uses Doxygen for API documentation. Run the following from the
repository root to generate docs in `docs/build`:

```bash
doxygen docs/Doxyfile
```
