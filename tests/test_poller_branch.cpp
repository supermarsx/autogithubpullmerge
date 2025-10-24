#include "github_poller.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <unordered_set>
#include <vector>

using namespace agpm;

class BranchListClient : public HttpClient {
public:
  int branch_get_count = 0;
  std::string last_deleted;
  std::string base = "https://api.github.com/repos/me/repo";
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    if (url == base)
      return "{\"default_branch\":\"main\"}";
    if (url == base + "/branches") {
      branch_get_count++;
      return "[{\"name\":\"main\"},{\"name\":\"feature\"}]";
    }
    return "[]";
  }
  HttpResponse
  get_with_headers(const std::string &url,
                   const std::vector<std::string> &headers) override {
    HttpResponse res;
    res.status_code = 200;
    res.body = get(url, headers);
    return res;
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    (void)headers;
    return "{}";
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    last_deleted = url;
    return "";
  }
};

class BranchCleanupClient : public HttpClient {
public:
  std::string base = "https://api.github.com/repos/me/repo";
  std::unordered_set<std::string> deleted;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    if (url == base)
      return "{\"default_branch\":\"main\"}";
    if (url == base + "/branches")
      return "[{\"name\":\"main\"},{\"name\":\"tmp/"
             "purge\"},{\"name\":\"feature\"}]";
    if (url == base + "/compare/main...feature")
      return "{\"status\":\"ahead\",\"ahead_by\":1}";
    if (url == base + "/compare/main...tmp/purge")
      return "{\"status\":\"identical\"}";
    if (url == base + "/pulls?state=closed")
      return "[{\"head\":{\"ref\":\"tmp/purge\"}}]";
    return "[]";
  }
  HttpResponse
  get_with_headers(const std::string &url,
                   const std::vector<std::string> &headers) override {
    HttpResponse res;
    res.status_code = 200;
    res.body = get(url, headers);
    return res;
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    (void)headers;
    return "{}";
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    deleted.insert(url);
    return "";
  }
};

class HeuristicBranchClient : public HttpClient {
public:
  std::string base = "https://api.github.com/repos/me/repo";
  int compare_requests = 0;
  int branch_metadata_requests = 0;

  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    if (url == base) {
      return R"({"default_branch":"main"})";
    }
    if (url == base + "/branches") {
      return R"([{"name":"main"},{"name":"feature-active"},{"name":"feature-identical"},{"name":"legacy/tmp"}])";
    }
    if (url == base + "/compare/main...feature-active") {
      ++compare_requests;
      return R"({"status":"ahead","ahead_by":3,"behind_by":0})";
    }
    if (url == base + "/compare/main...feature-identical") {
      ++compare_requests;
      return R"({"status":"identical","ahead_by":0,"behind_by":0})";
    }
    if (url == base + "/compare/main...legacy/tmp") {
      ++compare_requests;
      return R"({"status":"behind","ahead_by":0,"behind_by":4})";
    }
    if (url == base + "/branches/feature-active") {
      ++branch_metadata_requests;
      return R"({"name":"feature-active","commit":{"commit":{"committer":{"date":"2099-01-01T00:00:00Z"}}}})";
    }
    if (url == base + "/branches/feature-identical") {
      ++branch_metadata_requests;
      return R"({"name":"feature-identical","commit":{"commit":{"committer":{"date":"2015-01-01T00:00:00Z"}}}})";
    }
    if (url == base + "/branches/legacy/tmp") {
      ++branch_metadata_requests;
      return R"({"name":"legacy/tmp","commit":{"commit":{"committer":{"date":"2010-01-01T00:00:00Z"}}}})";
    }
    return "{}";
  }

  HttpResponse
  get_with_headers(const std::string &url,
                   const std::vector<std::string> &headers) override {
    HttpResponse res;
    res.status_code = 200;
    res.body = get(url, headers);
    return res;
  }

  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    (void)headers;
    return "{}";
  }

  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return "";
  }
};

TEST_CASE("test poller branch") {
  // Detect stray branches without cleanup
  {
    auto http = std::make_unique<BranchListClient>();
    BranchListClient *raw = http.get();
    GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
    GitHubPoller poller(client, {{"me", "repo"}}, 1000, 60, 0, 1, false, true);
    std::vector<std::vector<StrayBranch>> stray_snapshots;
    poller.set_stray_callback([&](const std::vector<StrayBranch> &branches) {
      stray_snapshots.push_back(branches);
    });
    std::vector<std::string> logs;
    poller.set_log_callback([&](const std::string &m) { logs.push_back(m); });
    poller.poll_now();
    REQUIRE(raw->branch_get_count == 1);
    bool stray_reported =
        std::any_of(logs.begin(), logs.end(), [](const auto &m) {
          return m.find("stray branches: 1") != std::string::npos;
        });
    REQUIRE(stray_reported);
    REQUIRE(raw->last_deleted.empty());
    REQUIRE_FALSE(stray_snapshots.empty());
    REQUIRE(stray_snapshots.back().size() == 1);
  }

  // Detect stray branches and delete them automatically
  {
    auto http = std::make_unique<BranchListClient>();
    BranchListClient *raw = http.get();
    GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
    GitHubPoller poller(client, {{"me", "repo"}}, 1000, 60, 0, 1, false, true,
                        agpm::StrayDetectionMode::RuleBased, false, "", false,
                        false, "", nullptr, {}, {}, false, nullptr, true);
    std::vector<std::vector<StrayBranch>> stray_snapshots;
    poller.set_stray_callback([&](const std::vector<StrayBranch> &branches) {
      stray_snapshots.push_back(branches);
    });
    poller.poll_now();
    REQUIRE(raw->branch_get_count == 1);
    REQUIRE_FALSE(stray_snapshots.empty());
    REQUIRE(stray_snapshots.back().empty());
  }

  // Cleanup branches and close dirty ones
  {
    auto http = std::make_unique<BranchCleanupClient>();
    BranchCleanupClient *raw = http.get();
    GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
    GitHubPoller poller(client, {{"me", "repo"}}, 1000, 60, 0, 1, false, true,
                        agpm::StrayDetectionMode::Combined, false, "tmp/");
    poller.set_branch_rule_action("stray", BranchAction::kDelete);
    poller.set_branch_rule_action("dirty", BranchAction::kDelete);
    poller.poll_now();
    REQUIRE(raw->deleted.count(raw->base + "/git/refs/heads/feature") == 1);
    REQUIRE(raw->deleted.count(raw->base + "/git/refs/heads/tmp/purge") == 1);
  }

  // Heuristic stray detection
  {
    auto http = std::make_unique<HeuristicBranchClient>();
    HeuristicBranchClient *raw = http.get();
    GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
    GitHubPoller poller(client, {{"me", "repo"}}, 1000, 60, 0, 1, false, false,
                        agpm::StrayDetectionMode::Heuristic);
    std::vector<std::string> logs;
    poller.set_log_callback([&](const std::string &m) { logs.push_back(m); });
    poller.poll_now();
    REQUIRE(raw->compare_requests == 3);
    REQUIRE(raw->branch_metadata_requests >= 2);
    bool heuristic_logged =
        std::any_of(logs.begin(), logs.end(), [](const auto &msg) {
          return msg.find("stray branches: 2") != std::string::npos;
        });
    REQUIRE(heuristic_logged);
  }
}
