#include "github_client.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace agpm;

class RuleHttpClient : public HttpClient {
public:
  std::string meta_response;
  std::string merge_response{"{\"merged\":true}"};
  int put_calls{0};
  std::string last_put_url;

  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    if (url.find("/pulls/") != std::string::npos) {
      return meta_response;
    }
    return "";
  }

  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)data;
    (void)headers;
    last_put_url = url;
    ++put_calls;
    return merge_response;
  }

  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return "";
  }
};

TEST_CASE("merge rules allow merge") {
  auto http = std::make_unique<RuleHttpClient>();
  http->meta_response =
      "{\"approvals\":2,\"mergeable\":true,\"mergeable_state\":\"clean\"}";
  RuleHttpClient *raw = http.get();
  GitHubClient client("tok", std::unique_ptr<HttpClient>(http.release()));
  client.set_required_approvals(1);
  client.set_require_status_success(true);
  client.set_require_mergeable_state(true);
  bool merged = client.merge_pull_request("o", "r", 1);
  REQUIRE(merged);
  REQUIRE(raw->put_calls == 1);
  REQUIRE(raw->last_put_url.find("/repos/o/r/pulls/1/merge") !=
          std::string::npos);
}

TEST_CASE("merge rules block approvals") {
  auto http = std::make_unique<RuleHttpClient>();
  http->meta_response =
      "{\"approvals\":0,\"mergeable\":true,\"mergeable_state\":\"clean\"}";
  RuleHttpClient *raw = http.get();
  GitHubClient client("tok", std::unique_ptr<HttpClient>(http.release()));
  client.set_required_approvals(1);
  client.set_require_status_success(true);
  client.set_require_mergeable_state(true);
  bool merged = client.merge_pull_request("o", "r", 1);
  REQUIRE_FALSE(merged);
  REQUIRE(raw->put_calls == 0);
}

TEST_CASE("merge rules block status") {
  auto http = std::make_unique<RuleHttpClient>();
  http->meta_response =
      "{\"approvals\":2,\"mergeable\":true,\"mergeable_state\":\"dirty\"}";
  RuleHttpClient *raw = http.get();
  GitHubClient client("tok", std::unique_ptr<HttpClient>(http.release()));
  client.set_required_approvals(1);
  client.set_require_status_success(true);
  client.set_require_mergeable_state(true);
  bool merged = client.merge_pull_request("o", "r", 1);
  REQUIRE_FALSE(merged);
  REQUIRE(raw->put_calls == 0);
}

TEST_CASE("merge rules block mergeable") {
  auto http = std::make_unique<RuleHttpClient>();
  http->meta_response =
      "{\"approvals\":2,\"mergeable\":false,\"mergeable_state\":\"clean\"}";
  RuleHttpClient *raw = http.get();
  GitHubClient client("tok", std::unique_ptr<HttpClient>(http.release()));
  client.set_required_approvals(1);
  client.set_require_status_success(true);
  client.set_require_mergeable_state(true);
  bool merged = client.merge_pull_request("o", "r", 1);
  REQUIRE_FALSE(merged);
  REQUIRE(raw->put_calls == 0);
}
