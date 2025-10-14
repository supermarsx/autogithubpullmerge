#include "notification.hpp"
#include <cstdlib>
#include <utility>

namespace agpm {

namespace {

[[maybe_unused]] std::string shell_escape(const std::string &s) {
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

[[maybe_unused]] std::string escape_apple_script(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '\\' || c == '"') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}

[[maybe_unused]] std::string escape_powershell(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '\'') {
      out += "''";
    } else {
      out.push_back(c);
    }
  }
  return out;
}

} // namespace

NotifySendNotifier::NotifySendNotifier(CommandRunner runner)
    : run_(std::move(runner)) {}

void NotifySendNotifier::notify(const std::string &message) {
#ifdef _WIN32
  std::string cmd =
      "powershell -NoProfile -Command \"Try {Import-Module BurntToast "
      "-ErrorAction Stop; New-BurntToastNotification -Text "
      "'autogithubpullmerge','" +
      escape_powershell(message) + "'} Catch {}\"";
  run_(cmd);
#elif defined(__APPLE__)
  if (run_("command -v terminal-notifier >/dev/null 2>&1") == 0) {
    std::string cmd =
        "terminal-notifier -title 'autogithubpullmerge' -message " +
        shell_escape(message);
    run_(cmd);
  } else {
    std::string cmd = "osascript -e 'display notification \"" +
                      escape_apple_script(message) +
                      "\" with title \"autogithubpullmerge\"'";
    run_(cmd);
  }
#elif defined(__linux__)
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
