#include "github_client.hpp"
#include "github_poller.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

using namespace agpm;

class HeadersMockHttp : public HttpClient {
public:
  std::string last_url;
  int hw_calls{0};
  std::string body;

  HttpResponse get_with_headers(const std::string &url,
                                const std::vector<std::string> &headers) override {
    (void)headers;
    last_url = url;
    ++hw_calls;
    return {body, {}, 200};
  }

  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    last_url = url;
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
};

TEST_CASE("single open PRs via REST one call") {
  auto mock = std::make_unique<HeadersMockHttp>();
  mock->body = R"([{"number":101,"title":"Fix bug"},{"number":102,"title":"Add tests"}])";
  HeadersMockHttp *raw = mock.get();
  GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(mock.release()));
  auto prs = client.list_open_pull_requests_single("me/repo");
  REQUIRE(prs.size() == 2);
  REQUIRE(prs[0].number == 101);
  REQUIRE(prs[0].owner == "me");
  REQUIRE(prs[0].repo == "repo");
  REQUIRE(raw->hw_calls == 1);
  REQUIRE(raw->last_url.find("/repos/me/repo/pulls?state=open") != std::string::npos);
}

TEST_CASE("single branches via REST one call") {
  auto mock = std::make_unique<HeadersMockHttp>();
  mock->body = R"([{"name":"main"},{"name":"feature/x"}])";
  HeadersMockHttp *raw = mock.get();
  GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(mock.release()));
  auto branches = client.list_branches_single("me/repo");
  REQUIRE(branches.size() == 2);
  REQUIRE(branches[0] == "main");
  REQUIRE(raw->hw_calls == 1);
  REQUIRE(raw->last_url.find("/repos/me/repo/branches?per_page=") != std::string::npos);
}

TEST_CASE("poller uses single open PR when rate low") {
  auto mock = std::make_unique<HeadersMockHttp>();
  // Return one PR; we check URL contains state=open
  mock->body = R"([{"number":7,"title":"Patch"}])";
  HeadersMockHttp *raw = mock.get();
  GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(mock.release()));
  std::vector<std::pair<std::string, std::string>> repos = {{"me", "repo"}};
  GitHubPoller poller(client, repos, 0, 1 /* max_rate <=1 forces single-call */);
  std::vector<PullRequest> seen;
  poller.set_pr_callback([&](const std::vector<PullRequest> &prs) { seen = prs; });
  poller.poll_now();
  REQUIRE(!seen.empty());
  REQUIRE(seen[0].number == 7);
  REQUIRE(raw->last_url.find("/pulls?state=open") != std::string::npos);
}

