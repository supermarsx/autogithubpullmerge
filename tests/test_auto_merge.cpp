#include "github_poller.hpp"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>

using namespace agpm;

class MergeHttpClient : public HttpClient {
public:
  MergeHttpClient() : merge_calls(0) {}
  std::atomic<int> merge_calls;
  std::string last_url;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    if (url.find("/pulls/") != std::string::npos) {
      return "{\"approvals\":2,\"mergeable\":true,\"mergeable_state\":"
             "\"clean\",\"state\":\"open\"}";
    }
    if (url.find("/pulls") != std::string::npos) {
      return "[{\"number\":1,\"title\":\"PR\"}]";
    }
    return "[]";
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)data;
    (void)headers;
    last_url = url;
    merge_calls++;
    return "{\"merged\":true}";
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return "";
  }
  std::string patch(const std::string &url, const std::string &data,
                    const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    (void)headers;
    return "";
  }
};

TEST_CASE("test auto merge") {
  auto http = std::make_unique<MergeHttpClient>();
  MergeHttpClient *raw = http.get();
  GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
  GitHubPoller poller(client, {{"me", "repo"}}, 50, 120, 0, 1, false, false,
                      false, "", true);
  std::vector<std::vector<PullRequest>> snapshots;
  poller.set_pr_callback(
      [&](const std::vector<PullRequest> &prs) { snapshots.push_back(prs); });
  poller.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  poller.stop();
  REQUIRE(raw->merge_calls > 0);
  REQUIRE(raw->last_url.find("/repos/me/repo/pulls/1/merge") !=
          std::string::npos);
  REQUIRE_FALSE(snapshots.empty());
  REQUIRE(snapshots.back().empty());
}

class DirtyHttpClient : public HttpClient {
public:
  std::atomic<int> close_calls{0};
  std::string last_patch_url;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    if (url.find("/pulls/") != std::string::npos) {
      return "{\"approvals\":0,\"mergeable\":false,\"mergeable_state\":"
             "\"dirty\",\"state\":\"open\"}";
    }
    if (url.find("/pulls") != std::string::npos) {
      return "[{\"number\":2,\"title\":\"Dirty PR\"}]";
    }
    return "[]";
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    (void)headers;
    return "{\"merged\":false}";
  }
  std::string patch(const std::string &url, const std::string &data,
                    const std::vector<std::string> &headers) override {
    (void)data;
    (void)headers;
    last_patch_url = url;
    close_calls++;
    return "{\"state\":\"closed\"}";
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return "";
  }
};

TEST_CASE("dirty pull requests are closed by rule engine") {
  auto http = std::make_unique<DirtyHttpClient>();
  DirtyHttpClient *raw = http.get();
  GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
  GitHubPoller poller(client, {{"me", "repo"}}, 50, 120, 0, 1, false, false,
                      false, "", true);
  std::vector<std::vector<PullRequest>> snapshots;
  poller.set_pr_callback(
      [&](const std::vector<PullRequest> &prs) { snapshots.push_back(prs); });
  poller.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  poller.stop();
  REQUIRE(raw->close_calls > 0);
  REQUIRE(raw->last_patch_url.find("/repos/me/repo/pulls/2") !=
          std::string::npos);
  REQUIRE_FALSE(snapshots.empty());
  REQUIRE(snapshots.back().empty());
}
