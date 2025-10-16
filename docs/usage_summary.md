#Usage Summary

This document highlights the main command line options, examples of
configuration files, logging behaviour, available TUI features, and how
to build the project on supported platforms.

## Supported Platforms

- Linux
- macOS
- Windows

## CLI Options

### General

- `-v/--verbose` - enable verbose output.
- `--config` - path to a YAML, TOML, or JSON configuration file.
- `--log-level` - set verbosity (`trace`, `debug`, `info`, `warn`, `error`,
  `critical`, `off`).
- `--history-db` - path to the SQLite history database.
- `--yes` - assume "yes" to confirmation prompts.

### Repository Filters

- `--include`/`--exclude` - repeatable repository include/exclude filters.
- `--include-merged` - show merged pull requests when listing (off by default).

### Authentication

- `--api-key` - specify a GitHub token on the command line.
- `--api-key-from-stream` - read tokens from standard input.
- `--api-key-url`/`--api-key-url-user`/`--api-key-url-password` - fetch tokens
  from a remote URL with optional basic authentication.
- `--api-key-file` - load tokens from a local YAML or JSON file.

### Polling

- `--poll-interval` - how often to poll GitHub (seconds, `0` disables).
- `--max-request-rate` - limit GitHub requests per minute.
- `--pr-limit` - limit how many pull requests to fetch when listing.
- `--pr-since` - only list pull requests newer than the given duration
  (e.g. `30m`, `2h`, `1d`).
- `--only-poll-prs` - only poll pull requests.
- `--only-poll-stray` - only poll stray branches.

### Networking

- `--download-limit` - limit download speed in bytes per second.
- `--upload-limit` - limit upload speed in bytes per second.
- `--max-download` - cap cumulative downloaded bytes.
- `--max-upload` - cap cumulative uploaded bytes.

### Actions

The following options perform destructive actions and require confirmation
(use `--yes` to skip the prompt):

- `--reject-dirty` - automatically close dirty stray branches.
- `--auto-merge` - merge pull requests automatically.
- `--purge-prefix` - delete branches with this prefix once their PR is
  closed or merged.
- `--purge-only` - skip pull request polling and only run branch cleanup.

Declining the confirmation prompt aborts the command and returns a non-zero
exit code.

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

### Purge-Only Mode

Combine `--purge-only` with `--purge-prefix` to delete branches without
polling for pull requests. This mode runs cleanup and exits without performing
any other polling activity.

### Overriding Dirty Branches

Branches that have diverged from their remote are considered dirty and are
skipped by default. Pass `--reject-dirty` to close these branches
automatically, overriding the protection for dirty branches.

## Rate Limiting

Use `--max-request-rate` to throttle GitHub API calls. This option limits how
many requests are issued per minute to avoid hitting GitHub's API quotas.

YAML:
```yaml
max_request_rate: 60
```

JSON:
```json
{
  "max_request_rate": 60
}
```

## Branch Protection Patterns

Glob patterns can designate branches that must never be modified. Provide
patterns with `--protect-branch` and use `--protect-branch-exclude` to remove
protection for specific names.

YAML:
```yaml
protected_branches:
  - main
  - release/*
protected_branch_excludes:
  - release/temp/*
```

JSON:
```json
{
  "protected_branches": ["main", "release/*"],
  "protected_branch_excludes": ["release/temp/*"]
}
```

## Dry-Run and Proxy Support

`--dry-run` simulates operations without altering repositories. HTTP requests
may be routed through proxies using `--http-proxy` and `--https-proxy`.

CLI:
```bash
autogithubpullmerge --dry-run --http-proxy http://proxy --https-proxy http://secureproxy
```

YAML:
```yaml
http_proxy: http://proxy
https_proxy: http://secureproxy
```

JSON:
```json
{
  "http_proxy": "http://proxy",
  "https_proxy": "http://secureproxy"
}
```

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

A curses-based TUI (`agpm::Tui`) displays active pull requests and recent log
messages. It uses ncurses on Linux and macOS and PDCurses on Windows, enabling
basic color output. The interface supports the following hotkeys:

- **↑/↓** – navigate pull requests
- **r** – refresh the list immediately
- **m** – merge the selected pull request
- **q** – quit the interface

## Building

See `dev/readme.md` for up-to-date vcpkg setup, build, test, lint and
documentation instructions for all platforms.
