#include "github_client.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <ctime>
#include <stdexcept>
#include <string>

using namespace agpm;

namespace {

class MockHttpClient : public HttpClient {
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
    last_url = url;
    last_method = "DELETE";
    return response;
  }
};

class InvalidJsonHttpClient : public HttpClient {
public:
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return "not json";
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    (void)headers;
    return "not json";
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return "";
  }
};

class ErrorHttpClient : public HttpClient {
public:
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    throw std::runtime_error("http error");
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    (void)headers;
    throw std::runtime_error("http error");
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    throw std::runtime_error("http error");
  }
};

class TimeoutHttpClient : public HttpClient {
public:
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    throw std::runtime_error("timeout");
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    (void)headers;
    throw std::runtime_error("timeout");
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    throw std::runtime_error("timeout");
  }
};

class MultiPageHttpClient : public HttpClient {
public:
  int calls = 0;
  std::string old_ts;
  std::string recent1_ts;
  std::string recent2_ts;

  MultiPageHttpClient(std::string old_t, std::string recent1_t,
                      std::string recent2_t)
      : old_ts(std::move(old_t)), recent1_ts(std::move(recent1_t)),
        recent2_ts(std::move(recent2_t)) {}

  HttpResponse
  get_with_headers(const std::string &url,
                   const std::vector<std::string> &headers) override {
    (void)headers;
    ++calls;
    if (calls == 1) {
      std::string body =
          "[{\"number\":1,\"title\":\"Old\",\"created_at\":\"" + old_ts +
          "\",\"updated_at\":\"" + old_ts +
          "\"},{\"number\":2,\"title\":\"New\",\"created_at\":\"" + old_ts +
          "\",\"updated_at\":\"" + recent1_ts + "\"}]";
      std::string next =
          url + (url.find('?') == std::string::npos ? "?" : "&") + "page=2";
      return {body, {"Link: <" + next + ">; rel=\"next\""}, 200};
    }
    std::string body = "[{\"number\":3,\"title\":\"Newer\",\"created_at\":\"" +
                       recent2_ts + "\",\"updated_at\":\"" + recent2_ts + "\"}]";
    return {body, {}, 200};
  }

  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    return get_with_headers(url, headers).body;
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

} // namespace

TEST_CASE("test github client") {
  // Test listing pull requests
  auto mock = std::make_unique<MockHttpClient>();
  mock->response = "[{\"number\":1,\"title\":\"Test\"}]";
  GitHubClient client({"token"}, std::unique_ptr<HttpClient>(mock.release()));
  auto prs = client.list_pull_requests("owner", "repo");
  REQUIRE(prs.size() == 1);
  REQUIRE(prs[0].number == 1);
  REQUIRE(prs[0].title == "Test");
  REQUIRE(prs[0].owner == "owner");
  REQUIRE(prs[0].repo == "repo");

  auto mock_include = std::make_unique<MockHttpClient>();
  mock_include->response = "[]";
  MockHttpClient *raw_inc = mock_include.get();
  GitHubClient client_inc({"token"},
                          std::unique_ptr<HttpClient>(mock_include.release()));
  client_inc.list_pull_requests("owner", "repo", true);
  REQUIRE(raw_inc->last_url.find("state=all") != std::string::npos);

  auto mock_limit = std::make_unique<MockHttpClient>();
  mock_limit->response = "[]";
  MockHttpClient *raw_limit = mock_limit.get();
  GitHubClient client_limit({"token"},
                            std::unique_ptr<HttpClient>(mock_limit.release()));
  client_limit.list_pull_requests("owner", "repo", false, 10);
  REQUIRE(raw_limit->last_url.find("per_page=10") != std::string::npos);

  using namespace std::chrono;
  auto now = system_clock::now();
  auto recent1 = now - minutes(30);
  auto recent2 = now - minutes(20);
  auto old = now - hours(5);
  auto recent1_t = system_clock::to_time_t(recent1);
  auto recent2_t = system_clock::to_time_t(recent2);
  auto old_t = system_clock::to_time_t(old);
  char recent_buf1[32];
  char recent_buf2[32];
  char old_buf[32];
  std::strftime(recent_buf1, sizeof(recent_buf1), "%Y-%m-%dT%H:%M:%SZ",
                std::gmtime(&recent1_t));
  std::strftime(recent_buf2, sizeof(recent_buf2), "%Y-%m-%dT%H:%M:%SZ",
                std::gmtime(&recent2_t));
  std::strftime(old_buf, sizeof(old_buf), "%Y-%m-%dT%H:%M:%SZ",
                std::gmtime(&old_t));
  auto multi_http = std::make_unique<MultiPageHttpClient>(
      std::string(old_buf), std::string(recent_buf1), std::string(recent_buf2));
  const MultiPageHttpClient *raw_multi = multi_http.get();
  GitHubClient client_multi({"tok"},
                            std::unique_ptr<HttpClient>(multi_http.release()));
  auto multi_prs =
      client_multi.list_pull_requests("me", "repo", false, 2, hours(1));
  REQUIRE(multi_prs.size() == 2);
  REQUIRE(multi_prs[0].number == 2);
  REQUIRE(multi_prs[1].number == 3);
  REQUIRE(raw_multi->calls == 2);

  // Test merging pull requests
  auto mock2 = std::make_unique<MockHttpClient>();
  mock2->response = "{\"merged\":true}";
  GitHubClient client2({"token"}, std::unique_ptr<HttpClient>(mock2.release()));
  bool merged = client2.merge_pull_request("owner", "repo", 1);
  REQUIRE(merged);

  // Invalid JSON should return empty results / false
  auto invalid = std::make_unique<InvalidJsonHttpClient>();
  GitHubClient client_invalid({"token"},
                              std::unique_ptr<HttpClient>(invalid.release()));
  auto bad_prs = client_invalid.list_pull_requests("owner", "repo");
  REQUIRE(bad_prs.empty());
  bool bad_merge = client_invalid.merge_pull_request("owner", "repo", 1);
  REQUIRE(!bad_merge);

  // HTTP errors should be swallowed and result in defaults
  auto throwing = std::make_unique<ErrorHttpClient>();
  GitHubClient client_throw({"token"},
                            std::unique_ptr<HttpClient>(throwing.release()));
  auto none_prs = client_throw.list_pull_requests("owner", "repo");
  REQUIRE(none_prs.empty());
  bool merge_fail = client_throw.merge_pull_request("owner", "repo", 1);
  REQUIRE(!merge_fail);

  // Timeouts should also be handled gracefully
  auto timeout = std::make_unique<TimeoutHttpClient>();
  GitHubClient client_timeout({"token"},
                              std::unique_ptr<HttpClient>(timeout.release()));
  auto timeout_prs = client_timeout.list_pull_requests("owner", "repo");
  REQUIRE(timeout_prs.empty());
  bool timeout_merge = client_timeout.merge_pull_request("owner", "repo", 1);
  REQUIRE(!timeout_merge);

  // Branch deletions percent-encode reserved characters in ref names.
  {
    auto mock_delete = std::make_unique<MockHttpClient>();
    MockHttpClient *raw_delete = mock_delete.get();
    raw_delete->response = "";
    GitHubClient delete_client(
        {"tok"}, std::unique_ptr<HttpClient>(mock_delete.release()));
    bool ok =
        delete_client.delete_branch("me", "repo", "feature/bug fix", {}, {});
    REQUIRE(ok);
    REQUIRE(raw_delete->last_method == "DELETE");
    REQUIRE(raw_delete->last_url == "https://api.github.com/repos/me/repo/git/"
                                    "refs/heads/feature%2Fbug%20fix");
  }
}
