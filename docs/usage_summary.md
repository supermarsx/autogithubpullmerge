#Usage Summary

This document highlights the main command line options, examples of
configuration files, logging behaviour, available TUI features, and how
to build the project on supported platforms.

## Supported Platforms

- Linux
- macOS
- Windows

## CLI Options

- `--config` - path to a YAML or JSON configuration file.
- `--log-level` - set verbosity (`trace`, `debug`, `info`, `warn`, `error`,
  `critical`, `off`).
- `--include`/`--exclude` - repeatable repository include/exclude filters.
- `--api-key` - specify a GitHub token on the command line.
- `--api-key-from-stream` - read tokens from standard input.
- `--api-key-url`/`--api-key-url-user`/`--api-key-url-password` - fetch tokens
  from a remote URL with optional basic authentication.
- `--api-key-file` - load tokens from a local YAML or JSON file.
- `--poll-interval` - how often to poll GitHub (seconds, `0` disables).
- `--max-request-rate` - limit GitHub requests per minute. When polling is
  enabled a worker thread fetches pull requests at the configured interval.

## Configuration File Examples

YAML:
```yaml
verbose: true
poll_interval: 60
max_request_rate: 100
```

JSON:
```json
{
  "verbose": false,
  "poll_interval": 30,
  "max_request_rate": 50
}
```

## Logging

Logging is controlled via `--log-level`. Valid levels are `trace`, `debug`,
`info`, `warn`, `error`, `critical` and `off`.

## TUI Features

A minimal ncurses-based TUI (`agpm::Tui`) is provided. It initializes ncurses,
draws a simple window that displays "AGPM running" and waits for a key
press before exiting.

## Building

Run the appropriate install, update and build scripts for your platform:

```bash
./scripts/install_linux.sh   # Linux
./scripts/update_libs.sh
./scripts/build_linux.sh
```

```bash
./scripts/install_mac.sh     # macOS
./scripts/update_libs.sh
./scripts/build_mac.sh
```

```powershell
./scripts/install_win.bat    # Windows
./scripts/update_libs.bat
./scripts/build_win.bat
```

Alternatively use the `compile_*` scripts for a g++ build without CMake.
