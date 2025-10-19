#Usage Summary

This document highlights the main command line options, examples of
configuration files, logging behaviour, available TUI features, and how
to build the project on supported platforms.

## Supported Platforms

- Linux
- macOS
- Windows

## CLI Options

Whenever possible each long option provides a single-character short alias.
For example, `-C` maps to `--config` and `-1` toggles `--only-poll-prs`.

### General

- `-v, --verbose` - enable verbose output.
- `--config` - path to a YAML, TOML, or JSON configuration file.
- `--history-db` - path to the SQLite history database.
- `--yes` - assume "yes" to confirmation prompts.
- `--demo-tui` - launch an interactive demo TUI with mock pull requests and
  branches.

### Logging

- `--log-level` - set verbosity (`trace`, `debug`, `info`, `warn`, `error`,
  `critical`, `off`).
- `--log-file` - write logs to a rotating file in addition to the console.
- `--log-limit` - throttle in-memory log history (default `200`).
- `--log-rotate N` - retain `N` rotated log files (default `3`, `0` disables rotation).
- `--log-compress` / `--no-log-compress` - toggle gzip compression for rotated
  log files (disabled by default).

### Repository Filters

- `--include`/`--exclude` - repeatable repository include/exclude filters (format `OWNER/REPO`).
- `--repo-discovery MODE` - `disabled` (default) uses explicit include lists; `all` discovers every repo accessible to the token; `filesystem` scans local git directories for GitHub remotes. Include/exclude always act as allow/deny lists.
- `--repo-discovery-root DIR` - repeatable directories scanned when using `filesystem` discovery.
- `--include-merged` - show merged pull requests when listing (off by default).

When discovery is disabled you must specify at least one repository with
`--include` or the equivalent configuration entry.
In configuration files, use the `repo_discovery_mode` key with values
`disabled`, `all`, or `filesystem`, and list directories under
`repo_discovery_roots` (or the single `repo_discovery_root`).

### Authentication

- `--api-key` - specify a GitHub token on the command line.
- `--api-key-from-stream` - read tokens from standard input.
- `--api-key-url`/`--api-key-url-user`/`--api-key-url-password` - fetch tokens
  from a remote URL with optional basic authentication.
- `--api-key-file` - repeatable; load tokens from local JSON/YAML/TOML files.
- `--auto-detect-token-files` - search the executable directory, configured
  repository roots, and common user directories (home, Documents, config
  folders) for token files.
- `--open-pat-page` - launch the GitHub PAT creation page in the default
  browser and exit immediately.
- `--save-pat FILE` - save a personal access token to `FILE`, prompting if no
  value is supplied.
- `--pat-value TOKEN` - provide the token inline when using `--save-pat` to
  avoid the interactive prompt.

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

Supported helpers:

- `prefix:<value>` (literal prefix match)
- `suffix:<value>` (literal suffix match)
- `contains:<value>` (literal substring match)
- `glob:<value>` or plain `*`/`?` wildcards
- `regex:<value>` (ECMAScript regex)
- `mixed:<value>` (wildcards plus regex tokens, e.g. `mixed:^release-*rc$`)

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

## Hotkey Configuration

- Toggle interactive shortcuts at startup with `--hotkeys on|off` or the
  configuration key `hotkeys.enabled`.
- Individual bindings can be disabled or reassigned via
  `hotkeys.bindings`. Provide either a string (comma or pipe separated) or an
  array of key names. Supported names include single characters (`r`), control
  chords (`Ctrl+R`), `Enter`, `Up`, `Down`, `Space`, and `Escape`. Use `none` or
  an empty value to disable an action, and `default` to keep the built-in
  mapping.

Example YAML:
```yaml
hotkeys:
  enabled: true
  bindings:
    refresh:
      - Ctrl+R
      - r
    merge: none
    details: "enter|d"
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
