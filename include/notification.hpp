#ifndef AUTOGITHUBPULLMERGE_NOTIFICATION_HPP
#define AUTOGITHUBPULLMERGE_NOTIFICATION_HPP

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
  /// Send a notification message to the user.
  virtual void notify(const std::string &message) = 0;
};

/**
 * Basic notifier that uses the `notify-send` command on Linux desktops.
 *
 * Other platforms simply ignore notifications.
 */
class NotifySendNotifier : public Notifier {
public:
  void notify(const std::string &message) override;
};

using NotifierPtr = std::shared_ptr<Notifier>;

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_NOTIFICATION_HPP
