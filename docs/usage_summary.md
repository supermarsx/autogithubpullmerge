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
- `--include-merged` - show merged pull requests when listing (off by default).
- `--only-poll-prs` - only poll pull requests.
- `--only-poll-stray` - only poll stray branches.
- `--reject-dirty` - automatically close dirty stray branches.
- `--auto-merge` - merge pull requests automatically.
- `--purge-prefix` - delete branches with this prefix once their PR is
  closed or merged.
- `--poll-interval` - how often to poll GitHub (seconds, `0` disables).
- `--max-request-rate` - limit GitHub requests per minute. When polling is
  enabled a worker thread fetches pull requests at the configured interval.
- `--pr-limit` - limit how many pull requests to fetch when listing.
- `--pr-since` - only list pull requests newer than the given duration
  (e.g. `30m`, `2h`, `1d`).

## Stray Branch Management

### Isolated Stray-Branch Purge Mode

Use `--only-poll-stray` to scan repositories exclusively for stray branches.
This isolated mode ignores pull requests and focuses solely on purging unused
branches.

### Integrated Branch Deletion

Supply `--purge-prefix` to remove branches with a matching prefix after their
pull request has been merged or closed. The cleanup is integrated into the
normal pull request workflow so branches disappear once they are no longer
needed.

### Overriding Dirty Branches

Branches that have diverged from their remote are considered dirty and are
skipped by default. Pass `--reject-dirty` to close these branches
automatically, overriding the protection for dirty branches.

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

A minimal curses-based TUI (`agpm::Tui`) is provided. It uses ncurses on
Linux and macOS and PDCurses on Windows. The interface initializes the
library, draws a simple window that displays "AGPM running" and waits for a
key press before exiting.

## Building

Run the appropriate install script for your platform to bootstrap vcpkg, then
build with the `vcpkg` CMake preset:

```bash
./scripts/install_linux.sh   # Linux
./scripts/install_mac.sh     # macOS
cmake --preset vcpkg
cmake --build --preset vcpkg
```

```powershell
./scripts/install_win.bat    # Windows
cmake --preset vcpkg
cmake --build --preset vcpkg --config Release
```

The default preset uses `VCPKG_ROOT` to locate vcpkg. To use a custom
installation, create or edit `CMakeUserPresets.json` or export `VCPKG_ROOT` and add it to
your `PATH`.
