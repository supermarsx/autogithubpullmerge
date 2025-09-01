#include "notification.hpp"
#include <cstdlib>

namespace agpm {

void NotifySendNotifier::notify(const std::string &message) {
#ifdef _WIN32
  std::string cmd =
      "powershell -NoProfile -Command \"Try {Import-Module BurntToast "
      "-ErrorAction "
      "Stop; New-BurntToastNotification -Text 'autogithubpullmerge','" +
      message + "'} Catch {}\"";
  std::system(cmd.c_str());
#elif __APPLE__
  if (std::system("command -v terminal-notifier >/dev/null 2>&1") == 0) {
    std::string cmd =
        "terminal-notifier -title 'autogithubpullmerge' -message '" + message +
        "'";
    std::system(cmd.c_str());
  } else {
    std::string cmd = "osascript -e 'display notification \"" + message +
                      "\" with title \"autogithubpullmerge\"'";
    std::system(cmd.c_str());
  }
#elif __linux__
  if (std::system("command -v notify-send >/dev/null 2>&1") == 0) {
    std::string cmd = "notify-send \"autogithubpullmerge\" \"" + message + "\"";
    std::system(cmd.c_str());
  }
#else
  (void)message;
#endif
}

} // namespace agpm
