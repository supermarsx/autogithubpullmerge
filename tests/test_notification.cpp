#include "notification.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

using namespace agpm;

TEST_CASE("NotifySendNotifier runs notify-send on Linux") {
#ifdef __linux__
  std::vector<std::string> cmds;
  NotifySendNotifier notifier([&](const std::string &cmd) {
    cmds.push_back(cmd);
    return 0;
  });
  notifier.notify("hello world");
  REQUIRE(cmds.size() == 2);
  CHECK(cmds[1] == "notify-send 'autogithubpullmerge' 'hello world'");
#endif
}

TEST_CASE("NotifySendNotifier uses BurntToast on Windows") {
#ifdef _WIN32
  std::vector<std::string> cmds;
  NotifySendNotifier notifier([&](const std::string &cmd) {
    cmds.push_back(cmd);
    return 0;
  });
  notifier.notify("hello world");
  REQUIRE(cmds.size() == 1);
  CHECK(cmds[0] == "powershell -NoProfile -Command \"Try {Import-Module "
                   "BurntToast -ErrorAction Stop; "
                   "New-BurntToastNotification -Text "
                   "'autogithubpullmerge','hello world'} Catch {}\"");
#endif
}

TEST_CASE("NotifySendNotifier prefers terminal-notifier on macOS") {
#ifdef __APPLE__
  std::vector<std::string> cmds;
  NotifySendNotifier notifier([&](const std::string &cmd) {
    cmds.push_back(cmd);
    if (cmd.find("command -v terminal-notifier") != std::string::npos) {
      return 0; // simulate presence of terminal-notifier
    }
    return 0;
  });
  notifier.notify("hello world");
  REQUIRE(cmds.size() == 2);
  CHECK(
      cmds[1] ==
      "terminal-notifier -title 'autogithubpullmerge' -message 'hello world'");
#endif
}

TEST_CASE("NotifySendNotifier does nothing on unsupported platforms") {
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__linux__)
  bool called = false;
  NotifySendNotifier notifier([&](const std::string &) {
    called = true;
    return 0;
  });
  notifier.notify("ignored");
  CHECK_FALSE(called);
#endif
}
