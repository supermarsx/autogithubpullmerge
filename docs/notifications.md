# Notifications

The project exposes a small notification interface so actions like merges or
branch purges can alert the user.

## Usage

Include `notification.hpp` and provide a notifier to the `GitHubPoller`:

```cpp
auto notifier = std::make_shared<agpm::NotifySendNotifier>();
poller.set_notifier(notifier);
```

`NotifySendNotifier` attempts to surface desktop notifications on the host
platform:

- **Linux** – uses `notify-send` when available.
- **Windows** – uses PowerShell's `New-BurntToastNotification` if the
  BurntToast module is installed.
- **macOS** – prefers `terminal-notifier` and falls back to `osascript`.

All message text is escaped before invoking external commands. If none of the
required utilities are present, the notification request is silently ignored.

## Extending

To support additional notification mechanisms, derive from `agpm::Notifier` and
implement the `notify` method. This allows integrating with alternative desktop
systems or external services such as email or chat bots.

## Hooks

The hook dispatcher complements desktop notifications by forwarding structured
events to local commands or remote HTTP endpoints on a background thread. Hooks
are disabled by default; enable them with `--enable-hooks` or by adding a
`hooks.enabled: true` stanza to the configuration file. Configure a command with
`--hook-command` or an endpoint with `--hook-endpoint`; both can be active at
the same time.

Each hook invocation receives a JSON payload containing the event name,
timestamp, and event-specific data. For HTTP targets the payload is delivered in
the request body with a `Content-Type: application/json` header. Command hooks
receive the payload through the `AGPM_HOOK_EVENT`, `AGPM_HOOK_COMMAND`, and
`AGPM_HOOK_PAYLOAD` environment variables, allowing scripts to react without
parsing command-line arguments.

Common events include:

- `pull_request.merged`, `pull_request.merge_failed`, `pull_request.closed`,
  and `pull_request.close_failed`.
- `branch.deleted` with the owning repository, branch name, and reason
  (`stray`, `new`, `purge`, or `purge_only`).
- `poll.pull_threshold` and `poll.branch_threshold` when aggregate counts
  exceed the configured limits.

Use `--hook-header` to add HTTP headers such as authentication tokens, and the
threshold flags to trigger alerts when repositories accumulate excessive pull
requests or stray branches.

Additional context can be provided to individual hook actions by defining a `parameters` object in the configuration. Parameter values are exposed via the JSON payload under `parameters` and, for command actions, through uppercased environment variables named `AGPM_HOOK_PARAM_<NAME>`.
