#include "github_poller.hpp"
#include <algorithm>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <iterator>
#include <thread>

using namespace agpm;

class CountHttpClient : public HttpClient {
public:
  explicit CountHttpClient(std::atomic<int> &c) : counter(c) {}
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    if (url.find("/rate_limit") == std::string::npos) {
      ++counter;
    }
    return "[]";
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

private:
  std::atomic<int> &counter;
};

TEST_CASE("test github poller") {
  std::atomic<int> count1{0};
  auto http1 = std::make_unique<CountHttpClient>(count1);
  GitHubClient client1({"tok"}, std::unique_ptr<HttpClient>(http1.release()));
  GitHubPoller poller1(client1, {{"me", "repo"}}, 50, 120, 0, 1);
  poller1.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(220));
  poller1.stop();
  REQUIRE(count1 >= 2); // should run multiple times

  std::atomic<int> count2{0};
  auto http2 = std::make_unique<CountHttpClient>(count2);
  GitHubClient client2({"tok"}, std::unique_ptr<HttpClient>(http2.release()));
  GitHubPoller poller2(client2, {{"me", "repo"}}, 50, 1, 0, 1);
  poller2.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(220));
  poller2.stop();
  REQUIRE(count2 == 1); // rate limited to first token
}

class JsonHttpClient : public HttpClient {
public:
  explicit JsonHttpClient(std::string b) : body(std::move(b)) {}
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return body;
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

private:
  std::string body;
};

class OverrideHttpClient : public HttpClient {
public:
  OverrideHttpClient(std::atomic<int> &pr_counter,
                     std::atomic<int> &branch_counter)
      : pr_requests_(pr_counter), branch_requests_(branch_counter) {}

  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    if (url.find("/rate_limit") != std::string::npos) {
      return "{}";
    }
    if (url.find("/pulls") != std::string::npos) {
      ++pr_requests_;
      return "[{\"number\":1,\"title\":\"T\",\"state\":\"open\"}]";
    }
    if (url.find("/branches") != std::string::npos) {
      ++branch_requests_;
      return "[{\"name\":\"main\"}]";
    }
    if (url.find("/repos/") != std::string::npos) {
      return "{\"default_branch\":\"main\"}";
    }
    return "{}";
  }

  std::string put(const std::string &, const std::string &,
                  const std::vector<std::string> &) override {
    return "{}";
  }

  std::string del(const std::string &, const std::vector<std::string> &) override {
    return "";
  }

private:
  std::atomic<int> &pr_requests_;
  std::atomic<int> &branch_requests_;
};

TEST_CASE("github poller sorts pull requests") {
  const std::string json =
      "[{\"number\":1,\"title\":\"PR2\"},{\"number\":2,\"title\":\"PR10\"},{"
      "\"number\":3,\"title\":\"PR1\"}]";
  std::vector<std::pair<std::string, std::string>> repos = {{"me", "repo"}};

  SECTION("alphanum") {
    auto http = std::make_unique<JsonHttpClient>(json);
    GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
    GitHubPoller poller(client, repos, 0, 60, 0, 1, true, false,
                        StrayDetectionMode::RuleBased, false, "", false, false,
                        "alphanum");
    std::vector<std::string> titles;
    poller.set_pr_callback([&](const std::vector<PullRequest> &prs) {
      titles.reserve(titles.size() + prs.size());
      std::transform(prs.begin(), prs.end(), std::back_inserter(titles),
                     [](const PullRequest &pr) { return pr.title; });
    });
    poller.poll_now();
    REQUIRE(titles == std::vector<std::string>{"PR1", "PR2", "PR10"});
  }

  SECTION("reverse") {
    auto http = std::make_unique<JsonHttpClient>(json);
    GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
    GitHubPoller poller(client, repos, 0, 60, 0, 1, true, false,
                        StrayDetectionMode::RuleBased, false, "", false, false,
                        "reverse");
    std::vector<std::string> titles;
    poller.set_pr_callback([&](const std::vector<PullRequest> &prs) {
      titles.reserve(titles.size() + prs.size());
      std::transform(prs.begin(), prs.end(), std::back_inserter(titles),
                     [](const PullRequest &pr) { return pr.title; });
    });
    poller.poll_now();
    REQUIRE(titles == std::vector<std::string>{"PR2", "PR10", "PR1"});
  }
}

TEST_CASE("repository overrides influence polling behaviour") {
  std::atomic<int> pr_requests{0};
  std::atomic<int> branch_requests{0};
  auto http = std::make_unique<OverrideHttpClient>(pr_requests, branch_requests);
  GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
  std::vector<std::pair<std::string, std::string>> repos = {{"me", "repo"}};
  agpm::RepositoryOptions opts;
  opts.only_poll_prs = true;
  opts.only_poll_stray = true;
  opts.purge_only = false;
  opts.auto_merge = false;
  opts.reject_dirty = false;
  opts.delete_stray = false;
  opts.purge_prefix = "";
  opts.hooks_enabled = false;
  agpm::RepositoryOptionsMap overrides;
  overrides.emplace("me/repo", opts);
  GitHubPoller poller(client, repos, 0, 60, 0, 1, false, false,
                      StrayDetectionMode::RuleBased, false, "", false, false,
                      "", nullptr, {}, {}, false, nullptr, false, 0.7,
                      std::chrono::seconds(60), false, 3, std::move(overrides));
  poller.poll_now();
  REQUIRE(pr_requests.load() >= 1);
  REQUIRE(branch_requests.load() == 0);
}
