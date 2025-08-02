#include "github_client.hpp"
#include <cassert>
#include <string>

using namespace agpm;

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

int main() {
  auto dummy = std::make_unique<DummyHttpClient>();
  dummy->response = "[{\"number\":2,\"title\":\"Another\"}]";
  GitHubClient client("token", std::unique_ptr<HttpClient>(dummy.release()));
  auto prs = client.list_pull_requests("octocat", "hello");
  assert(prs.size() == 1);
  assert(prs[0].number == 2);
  assert(prs[0].title == "Another");

  auto dummy2 = std::make_unique<DummyHttpClient>();
  dummy2->response = "{\"merged\":false}";
  GitHubClient client2("token", std::unique_ptr<HttpClient>(dummy2.release()));
  bool merged = client2.merge_pull_request("octocat", "hello", 5);
  assert(!merged);

  return 0;
}
