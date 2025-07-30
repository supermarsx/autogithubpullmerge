#include "github_client.hpp"
#include <cassert>
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

  // Test merging pull requests
  auto mock2 = std::make_unique<MockHttpClient>();
  mock2->response = "{\"merged\":true}";
  GitHubClient client2("token", std::unique_ptr<HttpClient>(mock2.release()));
  bool merged = client2.merge_pull_request("owner", "repo", 1);
  assert(merged);

  return 0;
}
