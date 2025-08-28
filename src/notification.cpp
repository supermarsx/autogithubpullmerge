#include "notification.hpp"
#include <cstdlib>

namespace agpm {

void NotifySendNotifier::notify(const std::string &message) {
#ifdef __linux__
  std::string cmd = "notify-send \"autogithubpullmerge\" \"" + message + "\"";
  std::system(cmd.c_str());
#else
  (void)message;
#endif
}

} // namespace agpm
