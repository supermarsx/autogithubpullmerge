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

TEST_CASE("github poller sorts pull requests") {
  const std::string json =
      "[{\"number\":1,\"title\":\"PR2\"},{\"number\":2,\"title\":\"PR10\"},{"
      "\"number\":3,\"title\":\"PR1\"}]";
  std::vector<std::pair<std::string, std::string>> repos = {{"me", "repo"}};

  SECTION("alphanum") {
    auto http = std::make_unique<JsonHttpClient>(json);
    GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
    GitHubPoller poller(client, repos, 0, 60, 0, 1, true, false, false, "",
                        false, false, "alphanum");
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
    GitHubPoller poller(client, repos, 0, 60, 0, 1, true, false, false, "",
                        false, false, "reverse");
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
