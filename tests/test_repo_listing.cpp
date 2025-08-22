#include "github_client.hpp"
#include "github_poller.hpp"
#include <catch2/catch_test_macros.hpp>
#include <utility>
#include <vector>

using namespace agpm;

class RepoHttpClient : public HttpClient {
public:
  int repo_calls = 0;
  int pr_calls = 0;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    if (url.find("/user/repos") != std::string::npos) {
      ++repo_calls;
      return "[{\"name\":\"repo\",\"owner\":{\"login\":\"me\"}}]";
    }
    if (url.find("/repos/me/repo/pulls") != std::string::npos) {
      ++pr_calls;
      return "[]";
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
};

TEST_CASE("list repositories and poll when none included") {
  auto http = std::make_unique<RepoHttpClient>();
  auto *raw = http.get();
  GitHubClient client("tok", std::unique_ptr<HttpClient>(http.release()));
  std::vector<std::pair<std::string, std::string>> repos;
  if (repos.empty())
    repos = client.list_repositories();
  REQUIRE(repos ==
          std::vector<std::pair<std::string, std::string>>{{"me", "repo"}});
  GitHubPoller poller(client, repos, 0, 60);
  poller.poll_now();
  REQUIRE(raw->repo_calls == 1);
  REQUIRE(raw->pr_calls == 1);
}
