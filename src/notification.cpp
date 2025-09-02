#include "notification.hpp"
#include <cstdlib>
#include <utility>

namespace agpm {

namespace {

std::string shell_escape(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 2);
  out.push_back('\'');
  for (char c : s) {
    if (c == '\'') {
      out += "'\\''";
    } else {
      out.push_back(c);
    }
  }
  out.push_back('\'');
  return out;
}

} // namespace

NotifySendNotifier::NotifySendNotifier(CommandRunner runner)
    : run_(std::move(runner)) {}

void NotifySendNotifier::notify(const std::string &message) {
#if defined(__linux__)
  if (run_("command -v notify-send >/dev/null 2>&1") == 0) {
    std::string cmd = "notify-send " + shell_escape("autogithubpullmerge") +
                      " " + shell_escape(message);
    run_(cmd);
  }
#else
  (void)message;
#endif
}

} // namespace agpm
