<img width="1536" height="1024" alt="intro-image" src="https://github.com/user-attachments/assets/afc97d8d-0dc9-4561-b7bc-e7518bfa82c5" />

[![GitHub Repo stars](https://img.shields.io/github/stars/supermarsx/autogithubpullmerge?style=flat-square)](https://github.com/supermarsx/autogithubpullmerge/stargazers)
[![GitHub watchers](https://img.shields.io/github/watchers/supermarsx/autogithubpullmerge?style=flat-square)](https://github.com/supermarsx/autogithubpullmerge/watchers)
[![GitHub forks](https://img.shields.io/github/forks/supermarsx/autogithubpullmerge?style=flat-square)](https://github.com/supermarsx/autogithubpullmerge/network/members)
[![GitHub issues](https://img.shields.io/github/issues/supermarsx/autogithubpullmerge?style=flat-square)](https://github.com/supermarsx/autogithubpullmerge/issues)
[![GitHub all releases](https://img.shields.io/github/downloads/supermarsx/autogithubpullmerge/total?style=flat-square)](https://github.com/supermarsx/autogithubpullmerge/releases)
[![License](https://img.shields.io/github/license/supermarsx/autogithubpullmerge?style=flat-square)](https://github.com/supermarsx/autogithubpullmerge/blob/main/license.md)
![Made with C++](https://img.shields.io/badge/Made%20with-C%2B%2B-00599C?style=flat-square&logo=c%2B%2B&logoColor=white)

A cross‑platform tool that both automates safe pull request merging and manages stray branches (detects, closes, deletes or purges), with a terminal UI and scriptable CLI.

> **Warning:** autogithubpullmerge can delete branches, merge pull requests, and modify repositories on your behalf. Misconfiguration or casual experimentation may cause irreversible changes across your organization. This tool is intended for experienced operators who understand the impact of every option they enable.

## Features
- Cross-platform build using CMake
- Linux, macOS and Windows install/build scripts
- Placeholder TUI application in C++23
- Dependencies managed with vcpkg
- Requires a curses development package for the TUI (`libncurses-dev` on
  Linux/macOS or `pdcurses` on Windows)
- Unit tests using Catch2
- SQLite-based history storage with `--history-db` and automatic CSV
  (`--export-csv`) or JSON (`--export-json`) export after each polling cycle
- Configurable logging with `--log-level` and optional `--log-file`
- Uses spdlog for colored console and rotating file logging
- Asynchronous hook dispatcher that forwards merge and branch events to custom
  commands or HTTP endpoints
- Cross-platform compile scripts (MSVC on Windows, g++ on Linux/macOS) with
  C++23 support
- Dependencies verified to build with C++23 via vcpkg
- CLI options for GitHub API keys (`--api-key`, `--api-key-from-stream`,
  `--api-key-url`, `--api-key-url-user`, `--api-key-url-password`,
  repeatable `--api-key-file` for JSON/YAML/TOML token files, and
  `--auto-detect-token-files` to locate tokens near the executable,
  repository roots, or common user directories)
- Repository discovery modes: manual OWNER/REPO lists by default, account or
  token discovery via `--repo-discovery all` (also accepts `account`/`token`),
  filesystem scanning with `--repo-discovery filesystem` and repeatable
  `--repo-discovery-root`, or combining both approaches via
  `--repo-discovery both`
- Supports YAML, TOML, and JSON configuration files
- Adaptive rate limiting with a configurable margin to protect GitHub API usage
- Branch protection patterns to guard important branches
- Dry-run mode and HTTP/HTTPS proxy support

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
`log_pattern`, `log_file`, `log_compress`, and `log_categories` values. Each
entry in `log_categories` maps a category (for example `logging`, `history`, or
`http`) to a log level. The CLI equivalent is the repeatable
`--log-category NAME[=LEVEL]` flag, which defaults the level to `debug` when
omitted.

## Notifications

`GitHubPoller` can emit user notifications when pull requests are merged or
branches are purged. A basic `NotifySendNotifier` uses the `notify-send`
command on Linux desktops. Implementations derive from `agpm::Notifier` to
integrate with other systems. See `docs/notifications.md` for details.

## Hooks

Enable hooks with `--enable-hooks` (or `hooks.enabled: true` in configuration)
to forward structured event payloads to shell commands or remote HTTP
endpoints. Use `--hook-command` to launch local automation, `--hook-endpoint`
to send JSON webhooks, and the `--hook-*-threshold` flags to trigger alerts when
repositories accumulate excessive pull requests or branches. The dispatcher runs
on a dedicated thread so hooks do not block polling. See the Hooks section in
`docs/notifications.md` for the full payload format and available events.

## API Key Options

See Authentication in "CLI Options (Reference)" below for the full list. If no
token is supplied via flags, the tool falls back to `GITHUB_TOKEN` (or
`AGPM_API_KEY`).

## Polling Options

See Polling, Branch Management, and Pull Request Management in
"CLI Options (Reference)" for the comprehensive list of flags and defaults.

## Networking

See Networking in "CLI Options (Reference)". Options can also be set via YAML
or JSON configuration files using matching keys.

<!-- Consolidated under CLI Options (Reference) -->

## Branch Protection Patterns

`--protect-branch` marks branches that must not be modified. Use
`--protect-branch-exclude` to remove protection for matching patterns.
Patterns accept tagged helpers:

- `prefix:<value>` – literal prefix match, e.g. `prefix:release/`.
- `suffix:<value>` – literal suffix match, e.g. `suffix:-stable`.
- `contains:<value>` – literal substring match.
- `glob:<value>` or plain `*`/`?` wildcards – glob expressions (`glob:hotfix-*`).
- `regex:<value>` – ECMAScript regular expressions (`regex:^feature-[0-9]+$`).
- `mixed:<value>` – combine wildcards with regex tokens; `*` expands to `.*`
  while regex anchors such as `^`/`$` are honoured (`mixed:^release-*rc$`).

Exclude patterns take precedence over protections.

### Command Line

```bash
autogithubpullmerge --protect-branch main \
  --protect-branch 'release/*' \
  --protect-branch '^hotfix-[0-9]+$' \
  --protect-branch 'prefix:hotfix/' \
  --protect-branch 'mixed:^support-*2024$' \
  --protect-branch-exclude 'suffix:-temp'
```

### Configuration

```yaml
protected_branches:
  - main
  - release/*
  - '^hotfix-[0-9]+$'
  - prefix:hotfix/
  - mixed:^support-*2024$
protected_branch_excludes:
  - suffix:-temp
```

```json
{
  "protected_branches": [
    "main",
    "release/*",
    "^hotfix-[0-9]+$",
    "prefix:hotfix/",
    "mixed:^support-*2024$"
  ],
  "protected_branch_excludes": ["suffix:-temp"]
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

## History Database and Export

`--history-db` sets the path to a SQLite database that records pull request
data each polling cycle. When `--export-csv` or `--export-json` are supplied,
the application writes the accumulated history to the given file after every
polling cycle.

```bash
autogithubpullmerge --history-db pr_history.db --export-csv pulls.csv
autogithubpullmerge --history-db pr_history.db --export-json pulls.json
```

The exports reflect the state captured at the end of each poll interval.

## TUI Hotkeys

The terminal interface supports the following key bindings:

- `r` refreshes the pull request list.
- `o` opens the selected pull request in a browser.
- `m` merges the selected pull request.
- `ENTER` or `d` shows details for the selected pull request.
- `q` quits the interface.
- Arrow keys navigate between pull requests.

## CI Status

[![Format](https://github.com/supermarsx/autogithubpullmerge/actions/workflows/format.yml/badge.svg?branch=main)](https://github.com/supermarsx/autogithubpullmerge/actions/workflows/format.yml)
[![Lint](https://github.com/supermarsx/autogithubpullmerge/actions/workflows/lint.yml/badge.svg?branch=main)](https://github.com/supermarsx/autogithubpullmerge/actions/workflows/lint.yml)
[![Typecheck](https://github.com/supermarsx/autogithubpullmerge/actions/workflows/typecheck.yml/badge.svg?branch=main)](https://github.com/supermarsx/autogithubpullmerge/actions/workflows/typecheck.yml)
[![Build](https://github.com/supermarsx/autogithubpullmerge/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/supermarsx/autogithubpullmerge/actions/workflows/build.yml)
[![Test](https://github.com/supermarsx/autogithubpullmerge/actions/workflows/test.yml/badge.svg?branch=main)](https://github.com/supermarsx/autogithubpullmerge/actions/workflows/test.yml)
[![Rolling Release](https://github.com/supermarsx/autogithubpullmerge/actions/workflows/rolling-release.yml/badge.svg?branch=main)](https://github.com/supermarsx/autogithubpullmerge/actions/workflows/rolling-release.yml)

## Examples

Example configuration files can be found in the `examples` directory:

- `config.example.yaml` (minimal)
- `config.example.json` (minimal)
- `config.example.toml` (minimal)
- `config.full.example.yaml` (full)
- `config.full.example.json` (full)
- `config.full.example.toml` (full)

To use these locally, copy an example to a real config filename in the repo root (these are gitignored):

```bash
cp examples/config.example.yaml config.yaml          # minimal YAML
cp examples/config.example.json config.json          # minimal JSON
cp examples/config.example.toml config.toml          # minimal TOML
cp examples/config.full.example.yaml config.yaml     # full YAML
cp examples/config.full.example.json config.json     # full JSON
cp examples/config.full.example.toml config.toml     # full TOML
cp examples/tokens.example.yaml tokens.yaml
```

You can combine configuration files with CLI flags:

```yaml
# examples/config.example.yaml
http_timeout: 45
download_limit: 1048576
```

```bash
autogithubpullmerge --config examples/config.example.yaml --http-retries 5 --upload-limit 512000
```

```toml
# examples/config.example.toml
http_timeout = 45
download_limit = 1048576
```

```bash
autogithubpullmerge --config examples/config.example.toml --http-retries 5 --upload-limit 512000
```

Or configure everything directly on the command line:

```bash
autogithubpullmerge --http-timeout 60 --http-retries 5 \
  --download-limit 1048576 --upload-limit 512000 \
  --max-download 10485760 --max-upload 5242880
```

## Development

For building, vcpkg setup, compiling, packaging, testing, linting, generating docs, and
local debugging, see:

- dev/readme.md

## CLI Options (Reference)

Whenever possible long-form options expose a single-character short alias.
Examples include `-C` for `--config`, `-1` for `--only-poll-prs`, and `-n`
for `--download-limit`.

General
- `-v, --verbose` Enable verbose output (implies debug level unless overridden).
- `--config FILE` Path to configuration file (YAML, TOML, or JSON).
- `--version` Show the current build's commit hash and date, then exit.
- `-y, --yes` Assume yes to confirmation prompts.
- `--dry-run` Perform a trial run with no changes.
- `--hotkeys on|off` Explicitly enable or disable interactive hotkeys.
- `--demo-tui` Launch an interactive mock TUI showcasing sample pull requests and
  branches.

Logging
- `--log-level LEVEL` Set logging level: `trace|debug|info|warn|error|critical|off` (default `info`).
- `--log-file FILE` Path to the on-disk rotating log.
- `--log-limit N` Max number of in-memory log messages shown in the TUI (default `200`).
- `--log-rotate N` Number of rotated log files to retain (default `3`, `0` disables rotation).
- `--log-compress` Enable gzip compression for rotated log files (disabled by default).
- `--log-sidecar` Show logs in a dedicated sidecar window beside the pull request list.
- `--log-category NAME[=LEVEL]` Enable a specific logging category and optionally set its level (defaults to `debug`). Repeat to enable multiple categories.

Repositories
- `--include REPO` Repository to include (repeatable). Format `OWNER/REPO`.
- `--exclude REPO` Repository to exclude (repeatable). Format `OWNER/REPO`.
- `--repo-discovery MODE` Control repository discovery; `disabled` (default)
  honors only includes, `all`/`account` pulls every repository visible to the
  token, `filesystem` scans local git directories for GitHub remotes, and
  `both` merges token and filesystem discovery.
- `--repo-discovery-root DIR` Directory to scan for git repositories (repeatable,
  used with the `filesystem` discovery mode).

When discovery is `disabled`, you must specify at least one repository via
`--include` (or the equivalent config entry). With discovery `all`, the tool
enumerates every accessible repository for the configured tokens and then uses
`include`/`exclude` lists to filter the results.
With discovery `filesystem`, the tool walks the provided directories looking for
git repositories whose origin remote points at GitHub and uses `include`/
`exclude` as allow/deny lists. `both` collects repositories from both token and
filesystem discovery before applying filters.
Configuration files control this behaviour with the `repo_discovery_mode`
setting (`disabled`, `all`, `filesystem`, or `both`) and `repo_discovery_roots`.

Branch Management
- `--protect-branch, --protected-branch PATTERN` Protect matching branches (repeatable). Glob or regex.
- `--protect-branch-exclude PATTERN` Remove protection for matching branches (repeatable).
- `--only-poll-stray` Poll only stray branches.
- `--stray-detection-engine MODE` Choose `rule`, `heuristic`, or `both` when
  analysing branches (defaults to `rule`).
- `--heuristic-stray-detection` Enable heuristics in addition to the default
  rule-based detection (performs additional GitHub API calls).
- `--reject-dirty` Close dirty stray branches automatically (dangerous).
- `--delete-stray` Delete stray branches without requiring a prefix (dangerous).
- `--allow-delete-base-branch` Permit deleting base branches such as `main` or `master` (very dangerous).
- `--purge-prefix PREFIX` Delete branches with this prefix after PR close/merge.
- `--purge-only` Only purge branches; skip PR polling (dangerous).

Set `stray_detection_engine: heuristic` or `both` in the configuration (for
example under a `workflow` section) to enable the heuristic engine by default.

Pull Request Management
- `--include-merged` Include merged PRs when listing.
- `--pr-limit N` Number of PRs to fetch when listing (default `50`).
- `--pr-since DURATION` Only list PRs newer than duration (e.g. `30m`, `2h`, `1d`).
- `--sort MODE` Sort titles: `alpha|reverse|alphanum|reverse-alphanum`.
- `--only-poll-prs` Poll only pull requests (skip branch operations).
- `--auto-merge` Automatically merge PRs (dangerous).
- `--require-approval N` Minimum approvals required before merge (default `0`).
- `--require-status-success` Require all status checks to pass before merge.
- `--require-mergeable` Require PR to be mergeable.

Authentication
- `--api-key TOKEN` Personal access token (repeatable; not recommended).
- `--api-key-from-stream` Read token(s) from stdin (one per line).
- `--api-key-url URL` Fetch token(s) from URL.
- `--api-key-url-user USER` Basic auth user for `--api-key-url`.
- `--api-key-url-password PASS` Basic auth password for `--api-key-url`.
- `--api-key-file FILE` Repeatable; load tokens from JSON/YAML/TOML files with
  `token`/`tokens` entries.
- `--auto-detect-token-files` Search the executable directory, configured
  repository roots, and common user folders (Documents, configuration
  directories) for token files.
- `--open-pat-page` Open the GitHub PAT creation page in your browser and exit.
- `--save-pat FILE` Save a PAT to `FILE`, prompting if no value is provided.
- `--pat-value TOKEN` Supply the PAT inline when using `--save-pat`.
- `--api-base URL` Base URL for GitHub API (default `https://api.github.com`).
  Note: if none are provided, falls back to `GITHUB_TOKEN` or `AGPM_API_KEY`.

History / Export
- `--history-db FILE` SQLite history database path (default `history.db`).
- `--export-csv FILE` Export PR history to CSV after each poll.
- `--export-json FILE` Export PR history to JSON after each poll.

Networking
- `--http-timeout SECONDS` HTTP request timeout (default `30`).
- `--http-retries N` Number of HTTP retry attempts (default `3`).
- `--download-limit BPS` Max download rate bytes/sec (0 = unlimited).
- `--upload-limit BPS` Max upload rate bytes/sec (0 = unlimited).
- `--max-download BYTES` Max cumulative download (0 = unlimited).
- `--max-upload BYTES` Max cumulative upload (0 = unlimited).
- `--http-proxy URL` HTTP proxy URL.
- `--https-proxy URL` HTTPS proxy URL.
- `--use-graphql` Use GraphQL API for pull requests.

Polling
- `--poll-interval SECONDS` Poll frequency; `0` disables background polling (default `0`).
- `--max-request-rate RATE` Max requests per minute (default `60`).
- `--max-hourly-requests RATE` Max requests per hour (default auto; falls back to
  `5000`/hour when detection fails and still honours the rate limit margin).
- `--rate-limit-margin FRACTION` Reserve a fraction of the hourly GitHub rate limit (default `0.7`, ~30% usage target).
- `--rate-limit-refresh-interval SECONDS` Frequency for querying GitHub's rate
  limit endpoint (default `60`).
- `--retry-rate-limit-endpoint` Continue polling the rate limit endpoint after a
  failure instead of switching permanently to the fallback limit.
- `--rate-limit-retry-limit N` Cap how many scheduled retries are attempted
  when the retry flag is enabled (default `3`).
- `--workers N` Number of worker threads (non-negative; default from config or 1).

Testing / Utilities
- `--single-open-prs OWNER/REPO` Fetch open PRs for a repo via one HTTP request and exit.
- `--single-branches OWNER/REPO` Fetch branches for a repo via one HTTP request and exit.

### Single Open-PR Poll (Testing)

Fetch the current open pull requests for a single repository using exactly one
HTTP request, then exit. This avoids pagination and is useful in tests/CI where
we want to minimize API calls.

```bash
# Token can come from GITHUB_TOKEN
autogithubpullmerge --single-open-prs owner/repo

# Example output
# owner/repo #123: Fix race in poller
# owner/repo #124: Add unit tests
# owner/repo pull requests: 2
```

Notes:
- `--single-open-prs` accepts `OWNER/REPO` format.
- Only open PRs are returned; merged/closed are ignored.
- This path bypasses the TUI and other polling activities.
- GitHub limits a single response to the first 100 open pull requests.

A small helper script is provided at `examples/single-open-prs.sh`.

### Single Branches Poll (Testing)

Fetch current branches for a single repository using exactly one HTTP request,
then exit. This avoids pagination and extra metadata calls.

```bash
# Token can come from GITHUB_TOKEN
autogithubpullmerge --single-branches owner/repo

# Example output
# owner/repo branch: main
# owner/repo branch: feature/new-ui
# owner/repo branches: 2
```

Notes:
- `--single-branches` accepts `OWNER/REPO` format.
- Returns branch names as reported by the first page of the REST API.
- This path bypasses the TUI and other polling activities.
- GitHub returns at most 100 branches per call for this endpoint.

A small helper script is provided at `examples/single-branches.sh`.
