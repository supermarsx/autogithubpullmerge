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
