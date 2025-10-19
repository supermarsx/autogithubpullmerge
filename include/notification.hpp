#ifndef AUTOGITHUBPULLMERGE_NOTIFICATION_HPP
#define AUTOGITHUBPULLMERGE_NOTIFICATION_HPP

#include <cstdlib>
#include <functional>
#include <memory>
#include <string>

namespace agpm {

/**
 * Interface for dispatching user notifications.
 *
 * Custom implementations may route messages to desktop systems,
 * logging facilities or external services.
 */
class Notifier {
public:
  virtual ~Notifier() = default;
  /**
   * Send a notification message to the user.
   *
   * @param message Textual message that should be surfaced to the user.
   */
  virtual void notify(const std::string &message) = 0;
};

/**
 * Desktop notifier that invokes platform-specific utilities:
 *
 * - Linux: `notify-send`
 * - Windows: BurntToast PowerShell module
 * - macOS: `terminal-notifier` (preferred) or `osascript`
 *
 * If the required tool is not available, the notification request is ignored.
 */
class NotifySendNotifier : public Notifier {
public:
  using CommandRunner = std::function<int(const std::string &)>;

  /**
   * Construct a notifier that executes platform-specific commands.
   *
   * @param runner Callback responsible for executing shell commands. The
   *        default implementation delegates to `std::system`.
   */
  explicit NotifySendNotifier(CommandRunner runner =
                                  [](const std::string &cmd) {
                                    return std::system(cmd.c_str());
                                  });

  /**
   * Dispatch a notification to the underlying platform-specific tool.
   *
   * @param message Textual message to deliver to the user.
   */
  void notify(const std::string &message) override;

private:
  CommandRunner run_;
};

using NotifierPtr = std::shared_ptr<Notifier>;

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_NOTIFICATION_HPP
