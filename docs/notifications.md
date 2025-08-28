# Notifications

The project exposes a small notification interface so actions like merges or
branch purges can alert the user.

## Usage

Include `notification.hpp` and provide a notifier to the `GitHubPoller`:

```cpp
auto notifier = std::make_shared<agpm::NotifySendNotifier>();
poller.set_notifier(notifier);
```

`NotifySendNotifier` uses the `notify-send` command available on many Linux
systems to display desktop notifications. Other platforms ignore the call.

## Extending

To support additional notification mechanisms, derive from `agpm::Notifier` and
implement the `notify` method. This allows integrating with alternative desktop
systems or external services such as email or chat bots.
