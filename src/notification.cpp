/**
 * @file notification.cpp
 * @brief Implements cross-platform desktop notification dispatch for
 * autogithubpullmerge.
 *
 * This file defines the NotifySendNotifier class and helper functions for
 * sending user notifications using platform-specific utilities (notify-send,
 * terminal-notifier, osascript, PowerShell/BurntToast).
 */
#include "notification.hpp"
#include <cstdlib>
#include <utility>

namespace agpm {

namespace {

/**
 * Quote a string for safe use in POSIX shells.
 *
 * @param s String to escape.
 * @return Safely quoted representation suitable for `sh`/`bash`.
 */
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

/**
 * Escape characters for use inside AppleScript quoted strings.
 *
 * @param s Raw string to escape.
 * @return Escaped string that can be embedded in AppleScript quotes.
 */
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

/**
 * Escape characters for use inside PowerShell single-quoted strings.
 *
 * @param s Raw string to escape.
 * @return Escaped string safe for PowerShell single-quoted literals.
 */
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

/**
 * Construct a notifier using the provided command runner.
 *
 * @param runner Callback responsible for executing notification commands.
 */
NotifySendNotifier::NotifySendNotifier(CommandRunner runner)
    : run_(std::move(runner)) {}

/**
 * Dispatch a desktop notification using platform-specific utilities.
 *
 * @param message Textual message to present to the user.
 */
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
