#include "github_client.hpp"
#include <cassert>
#include <stdexcept>
#include <string>

using namespace agpm;

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

class ThrowingHttpClient : public HttpClient {
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

int main() {
  // Test listing pull requests
  auto mock = std::make_unique<MockHttpClient>();
  mock->response = "[{\"number\":1,\"title\":\"Test\"}]";
  GitHubClient client("token", std::unique_ptr<HttpClient>(mock.release()));
  auto prs = client.list_pull_requests("owner", "repo");
  assert(prs.size() == 1);
  assert(prs[0].number == 1);
  assert(prs[0].title == "Test");

  auto mock_include = std::make_unique<MockHttpClient>();
  mock_include->response = "[]";
  MockHttpClient *raw_inc = mock_include.get();
  GitHubClient client_inc("token",
                          std::unique_ptr<HttpClient>(mock_include.release()));
  client_inc.list_pull_requests("owner", "repo", true);
  assert(raw_inc->last_url.find("state=all") != std::string::npos);

  auto mock_limit = std::make_unique<MockHttpClient>();
  mock_limit->response = "[]";
  MockHttpClient *raw_limit = mock_limit.get();
  GitHubClient client_limit("token",
                            std::unique_ptr<HttpClient>(mock_limit.release()));
  client_limit.list_pull_requests("owner", "repo", false, 10);
  assert(raw_limit->last_url.find("per_page=10") != std::string::npos);

  // Test merging pull requests
  auto mock2 = std::make_unique<MockHttpClient>();
  mock2->response = "{\"merged\":true}";
  GitHubClient client2("token", std::unique_ptr<HttpClient>(mock2.release()));
  bool merged = client2.merge_pull_request("owner", "repo", 1);
  assert(merged);

  // Invalid JSON should return empty results / false
  auto invalid = std::make_unique<InvalidJsonHttpClient>();
  GitHubClient client_invalid("token",
                              std::unique_ptr<HttpClient>(invalid.release()));
  auto bad_prs = client_invalid.list_pull_requests("owner", "repo");
  assert(bad_prs.empty());
  bool bad_merge = client_invalid.merge_pull_request("owner", "repo", 1);
  assert(!bad_merge);

  // HTTP errors should be swallowed and result in defaults
  auto throwing = std::make_unique<ThrowingHttpClient>();
  GitHubClient client_throw("token",
                            std::unique_ptr<HttpClient>(throwing.release()));
  auto none_prs = client_throw.list_pull_requests("owner", "repo");
  assert(none_prs.empty());
  bool merge_fail = client_throw.merge_pull_request("owner", "repo", 1);
  assert(!merge_fail);

  return 0;
}
