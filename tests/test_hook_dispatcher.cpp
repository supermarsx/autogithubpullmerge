#include "hook.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <nlohmann/json.hpp>

using namespace std::chrono_literals;

TEST_CASE("hook dispatcher sends command events") {
  agpm::HookSettings settings;
  settings.enabled = true;
  agpm::HookAction action;
  action.type = agpm::HookActionType::Command;
  action.command = "echo";
  action.parameters.emplace_back("branch", "feature");
  settings.default_actions.push_back(action);

  std::mutex mutex;
  std::condition_variable cv;
  bool received = false;
  nlohmann::json payload;

  agpm::HookDispatcher dispatcher(
      settings,
      [&](const agpm::HookAction &act, const agpm::HookEvent &evt,
          const std::string &body) {
        std::unique_lock<std::mutex> lock(mutex);
        payload = nlohmann::json::parse(body);
        REQUIRE(act.command == "echo");
        REQUIRE(act.parameters.size() == 1);
        REQUIRE(act.parameters.front().first == "branch");
        REQUIRE(payload["parameters"]["branch"] == "feature");
        REQUIRE(evt.name == "pull_request.merged");
        received = true;
        cv.notify_one();
        return 0;
      });

  dispatcher.enqueue(agpm::HookEvent{
      "pull_request.merged",
      { {"number", 7}, {"owner", "octocat"}, {"repo", "hello"} }});

  std::unique_lock<std::mutex> lock(mutex);
  REQUIRE(cv.wait_for(lock, 500ms, [&] { return received; }));
  REQUIRE(payload["event"] == "pull_request.merged");
  REQUIRE(payload["data"]["number"] == 7);
  REQUIRE(payload["data"]["owner"] == "octocat");
}

TEST_CASE("hook dispatcher sends http events") {
  agpm::HookSettings settings;
  settings.enabled = true;
  agpm::HookAction action;
  action.type = agpm::HookActionType::Http;
  action.endpoint = "https://example.test/hook";
  action.method = "POST";
  action.parameters.emplace_back("severity", "warning");
  settings.default_actions.push_back(action);

  std::mutex mutex;
  std::condition_variable cv;
  bool received = false;
  std::string last_endpoint;
  nlohmann::json payload;

  agpm::HookDispatcher dispatcher(
      settings,
      agpm::HookDispatcher::CommandExecutor{},
      [&](const agpm::HookAction &act, const agpm::HookEvent &evt,
          const std::string &body) {
        std::unique_lock<std::mutex> lock(mutex);
        REQUIRE(evt.name == "poll.branch_threshold");
        last_endpoint = act.endpoint;
        REQUIRE(act.parameters.size() == 1);
        payload = nlohmann::json::parse(body);
        REQUIRE(payload.contains("parameters"));
        REQUIRE(payload["parameters"]["severity"] == "warning");
        received = true;
        cv.notify_one();
        return 202L;
      });

  dispatcher.enqueue(agpm::HookEvent{
      "poll.branch_threshold",
      { {"total_branches", 42}, {"threshold", 10} }});

  std::unique_lock<std::mutex> lock(mutex);
  REQUIRE(cv.wait_for(lock, 500ms, [&] { return received; }));
  REQUIRE(last_endpoint == "https://example.test/hook");
  REQUIRE(payload["data"]["total_branches"] == 42);
  REQUIRE(payload["data"]["threshold"] == 10);
}
