#include "github_client.hpp"
#include <cassert>
#include <string>
#include <vector>

using namespace agpm;

class HeaderHttpClient : public HttpClient {
public:
  std::vector<std::string> last_headers;
  std::string response;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    last_headers = headers;
    return response;
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    last_headers = headers;
    return response;
  }
};

int main() {
  auto http = std::make_unique<HeaderHttpClient>();
  http->response = "[]";
  HeaderHttpClient *raw = http.get();
  GitHubClient client("token123", std::unique_ptr<HttpClient>(http.release()));
  client.list_pull_requests("owner", "repo");
  bool found = false;
  for (const auto &h : raw->last_headers) {
    if (h == "Authorization: token token123")
      found = true;
  }
  assert(found);

  auto http2 = std::make_unique<HeaderHttpClient>();
  http2->response = "{\"merged\":true}";
  HeaderHttpClient *raw2 = http2.get();
  GitHubClient client2("tok", std::unique_ptr<HttpClient>(http2.release()));
  bool merged = client2.merge_pull_request("owner", "repo", 1);
  assert(merged);
  found = false;
  for (const auto &h : raw2->last_headers) {
    if (h == "Authorization: token tok")
      found = true;
  }
  assert(found);

  return 0;
}
