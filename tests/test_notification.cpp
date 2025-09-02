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

TEST_CASE("NotifySendNotifier does nothing on non-Linux") {
#ifndef __linux__
  bool called = false;
  NotifySendNotifier notifier([&](const std::string &) {
    called = true;
    return 0;
  });
  notifier.notify("ignored");
  CHECK_FALSE(called);
#endif
}
