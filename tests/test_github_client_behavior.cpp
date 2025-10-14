#include "github_client.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <unordered_map>

using namespace agpm;

namespace {

class DummyHttpClient : public HttpClient {
public:
  std::string last_url;
  std::string last_method;
  std::string response;

  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    last_url = url;
    last_method = "GET";
    return response;
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)data;
    (void)headers;
    last_url = url;
    last_method = "PUT";
    return response;
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    last_method = "DELETE";
    return response;
  }
};

class BranchHttpClient : public HttpClient {
public:
  std::unordered_map<std::string, std::string> responses;
  std::string last_deleted;
  std::string last_url;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    last_url = url;
    auto it = responses.find(url);
    if (it != responses.end()) {
      return it->second;
    }
    return "{}";
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

} // namespace

TEST_CASE("test github client behavior") {
  auto dummy = std::make_unique<DummyHttpClient>();
  dummy->response = "[{\"number\":2,\"title\":\"Another\"}]";
  GitHubClient client({"token"}, std::unique_ptr<HttpClient>(dummy.release()));
  auto prs = client.list_pull_requests("octocat", "hello");
  REQUIRE(prs.size() == 1);
  REQUIRE(prs[0].number == 2);
  REQUIRE(prs[0].title == "Another");

  auto dummy2 = std::make_unique<DummyHttpClient>();
  dummy2->response = "{\"merged\":false}";
  GitHubClient client2({"token"},
                       std::unique_ptr<HttpClient>(dummy2.release()));
  bool merged = client2.merge_pull_request("octocat", "hello", 5);
  REQUIRE(!merged);

  // Clean branch should not be deleted.
  {
    auto http = std::make_unique<BranchHttpClient>();
    BranchHttpClient *raw = http.get();
    std::string base = "https://api.github.com/repos/me/repo";
    raw->responses[base] = "{\"default_branch\":\"main\"}";
    raw->responses[base + "/branches"] =
        "[{\"name\":\"main\"},{\"name\":\"feature\"}]";
    raw->responses[base + "/compare/main...feature"] =
        "{\"status\":\"identical\",\"ahead_by\":0}";
    GitHubClient branch_client({"tok"},
                               std::unique_ptr<HttpClient>(http.release()));
    branch_client.close_dirty_branches("me", "repo");
    REQUIRE(raw->last_deleted.empty());
  }

  // Dirty branch should be deleted.
  {
    auto http = std::make_unique<BranchHttpClient>();
    BranchHttpClient *raw = http.get();
    std::string base = "https://api.github.com/repos/me/repo";
    raw->responses[base] = "{\"default_branch\":\"main\"}";
    raw->responses[base + "/branches"] =
        "[{\"name\":\"main\"},{\"name\":\"feature\"}]";
    raw->responses[base + "/compare/main...feature"] =
        "{\"status\":\"ahead\",\"ahead_by\":1}";
    GitHubClient branch_client({"tok"},
                               std::unique_ptr<HttpClient>(http.release()));
    branch_client.close_dirty_branches("me", "repo");
    REQUIRE(raw->last_deleted == base + "/git/refs/heads/feature");
  }
}
