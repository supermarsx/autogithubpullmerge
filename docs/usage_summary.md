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
- `--version` - print the current build's commit hash and date, then exit.
- `--yes` - assume "yes" to confirmation prompts.
- `--demo-tui` - launch an interactive demo TUI with mock pull requests and
  branches.

### Logging

- `--log-level` - set verbosity (`trace`, `debug`, `info`, `warn`, `error`,
  `critical`, `off`).
- `--log-file` - write logs to a rotating file in addition to the console.
- `--log-limit` - throttle in-memory log history (default `200`).
- `--log-rotate N` - retain `N` rotated log files (default `3`, `0` disables rotation).
- `--log-compress` - enable gzip compression for rotated log files (disabled by default).
- `--log-sidecar` - open a dedicated log window alongside pull request details.
- `--request-caddy-window` - display request queue status, latency, and budget
  projections in a sidecar window.
- `--log-category NAME[=LEVEL]` - enable a logging category with an optional level override (defaults to `debug`). Repeat to
  enable multiple categories.

### Hooks

- `--enable-hooks` / `--disable-hooks` - toggle the asynchronous hook
  dispatcher.
- `--hook-command COMMAND` - execute `COMMAND` with hook metadata exposed via
  environment variables.
- `--hook-endpoint URL` - send hook events to `URL` with a JSON payload.
- `--hook-method METHOD` - override the HTTP verb used when calling the
  configured endpoint (defaults to `POST`).
- `--hook-header NAME=VALUE` - add HTTP headers to outgoing hook requests;
  repeatable.
- `--hook-pull-threshold N` - emit a hook event when the aggregated pull request
  count exceeds `N`.
- `--hook-branch-threshold N` - emit a hook event when the aggregated branch
  count exceeds `N`.

### Integrations

- `--mcp-server` - start the Model Context Protocol (MCP) server on a
  background thread so external tools can query repositories, pull requests, and
  branches or trigger merges and cleanups.
- `--mcp-server-bind ADDR` - bind the MCP listener to `ADDR` (default
  `127.0.0.1`).
- `--mcp-server-port PORT` - listen on TCP `PORT` (default `7332`).
- `--mcp-server-backlog N` - allow up to `N` pending connections (default
  `16`).
- `--mcp-server-max-clients N` - serve at most `N` clients before recycling the
  listener (`0` = unlimited).
- `--mcp-caddy-window` - display a sidecar window streaming MCP requests and
  responses in the TUI.

### Repository Filters

- `--include`/`--exclude` - repeatable repository include/exclude filters (format `OWNER/REPO`).
- `--repo-discovery MODE` - `disabled` (default) uses explicit include lists; `all`/`account` discovers every repo accessible to the token; `filesystem` scans local git directories for GitHub remotes; `both` merges token and filesystem discovery. Include/exclude always act as allow/deny lists.
- `--repo-discovery-root DIR` - repeatable directories scanned when using `filesystem` discovery.
- `--include-merged` - show merged pull requests when listing (off by default).

When discovery is disabled you must specify at least one repository with
`--include` or the equivalent configuration entry.
In configuration files, use the `repo_discovery_mode` key with values
`disabled`, `all`, `filesystem`, or `both`, and list directories under
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
- `--max-hourly-requests` - cap GitHub requests per hour (auto-detected with a
  `5000`/hour fallback when the API is unavailable).
- `--rate-limit-margin` - reserve a fraction of the hourly GitHub rate limit (default `0.7`).
- `--rate-limit-refresh-interval` - query GitHub's rate limit endpoint every
  _N_ seconds (default `60`).
- `--retry-rate-limit-endpoint` - keep retrying the rate limit endpoint after a
  failure instead of falling back permanently.
- `--rate-limit-retry-limit` - cap how many scheduled retries are attempted
  when `--retry-rate-limit-endpoint` is supplied (default `3`).
- `--pr-limit` - limit how many pull requests to fetch when listing.
- `--pr-since` - only list pull requests newer than the given duration
  (e.g. `30m`, `2h`, `1d`).
- `--only-poll-prs` - only poll pull requests.
- `--only-poll-stray` - only poll stray branches.
- `--stray-detection-engine` - select `rule`, `heuristic`, or `both` for stray
  branch analysis (defaults to `rule`).
- `--heuristic-stray-detection` - shorthand to enable heuristics in addition to
  the default rule-based analysis (performs extra API calls).

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

### Stray Detection Engines

The poller always runs a deterministic rule-based engine to flag likely stray
branches. Supply `--stray-detection-engine` (or set
`stray_detection_engine: rule|heuristic|both` in the configuration) to switch to
heuristics-only analysis or to combine both engines. The
`--heuristic-stray-detection` flag is preserved for convenience and simply
enables the heuristic engine alongside the rule-based checks. When heuristics
are active the poller compares each branch against the repository's default
branch and inspects recent commit activity. Branches that are fully merged, fall
behind without unique commits, or have gone stale are flagged as stray in the
summary output. The additional context requires a handful of extra API calls per
branch scan.

### Purge-Only Mode

Combine `--purge-only` with `--purge-prefix` to delete branches without
polling for pull requests. This mode runs cleanup and exits without performing
any other polling activity.

### Overriding Dirty Branches

Branches that have diverged from their remote are considered dirty and are
skipped by default. Pass `--reject-dirty` to close these branches
automatically, overriding the protection for dirty branches.

## Rate Limiting

Use `--max-request-rate` to throttle GitHub API calls and
`--max-hourly-requests` to enforce an absolute hourly ceiling. The poller also
monitors GitHub's `/rate_limit` endpoint at the configured refresh interval and
uses a FIFO request scheduler to pace calls. Each completed request updates a
smoothed average that drives the scheduler's per-request delay, keeping usage
comfortably below the configured margin (70% reserved by default, targeting 30%
utilisation). When the rate limit endpoint fails the poller falls back to a
5,000 requests/hour budget; it stops querying the endpoint after the first
failure unless `--retry-rate-limit-endpoint` is supplied, in which case it will
retry up to `--rate-limit-retry-limit` times before giving up.

The scheduler raises a warning when the backlog grows large and includes an
estimated time to drain outstanding requests based on the current rate budget.

Tune the reserve with `--rate-limit-margin` or the matching configuration key
and adjust the refresh interval and retry behaviour via the new rate limit
flags to leave additional headroom for merge operations and branch maintenance.

YAML:
```yaml
rate_limits:
  max_request_rate: 60               # Requests per minute cap enforced by scheduler
  max_hourly_requests: 2000          # Hard hourly ceiling (0 autodetects via API)
  rate_limit_margin: 0.7             # Fraction of hourly budget reserved as safety buffer
  rate_limit_refresh_interval: 60    # Seconds between /rate_limit checks
  retry_rate_limit_endpoint: false   # Continue probing the endpoint after the first failure
  rate_limit_retry_limit: 3          # Maximum scheduled retries when retries are enabled
```

JSON:
```json
{
  "rate_limits": {
    "max_request_rate": 60,
    "max_hourly_requests": 2000,
    "rate_limit_margin": 0.7,
    "rate_limit_refresh_interval": 60,
    "retry_rate_limit_endpoint": false,
    "rate_limit_retry_limit": 3
  }
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
network:
  http_proxy: http://proxy
  https_proxy: http://secureproxy
```

JSON:
```json
{
  "network": {
    "http_proxy": "http://proxy",
    "https_proxy": "http://secureproxy"
  }
}
```

## Configuration File Examples

Configuration entries are organised into nested sections (for example `core`,
`rate_limits`, `network`, and `logging`) so related settings stay together
regardless of format. Each nested group expands to the same keys exposed by
the CLI options described above.

YAML:
```yaml
core:
  verbose: true
  poll_interval: 60
mcp:
  enabled: true
  bind_address: 0.0.0.0
  port: 7332
  backlog: 32
  max_clients: 8
  caddy_window: true
rate_limits:
  max_request_rate: 100
```

JSON:
```json
{
  "core": {
    "verbose": false,
    "poll_interval": 30
  },
  "mcp": {
    "enabled": true,
    "bind_address": "0.0.0.0",
    "port": 7332,
    "backlog": 32,
    "max_clients": 8,
    "caddy_window": true
  },
  "rate_limits": {
    "max_request_rate": 50
  }
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
