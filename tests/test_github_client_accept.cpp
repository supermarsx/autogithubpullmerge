#include "github_client.hpp"
#include <cassert>
#include <string>
#include <vector>

using namespace agpm;

class HeaderCaptureHttp : public HttpClient {
public:
  std::vector<std::string> last_headers;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    last_headers = headers;
    return "[]";
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    last_headers = headers;
    return "{\"merged\":true}";
  }
};

int main() {
  auto http = std::make_unique<HeaderCaptureHttp>();
  HeaderCaptureHttp *raw = http.get();
  GitHubClient client("tok", std::unique_ptr<HttpClient>(http.release()));
  client.list_pull_requests("octocat", "hello");
  bool has_accept = false;
  for (const auto &h : raw->last_headers) {
    if (h == "Accept: application/vnd.github+json")
      has_accept = true;
  }
  assert(has_accept);

  auto http2 = std::make_unique<HeaderCaptureHttp>();
  HeaderCaptureHttp *raw2 = http2.get();
  GitHubClient client2("tok2", std::unique_ptr<HttpClient>(http2.release()));
  client2.merge_pull_request("octocat", "hello", 1);
  has_accept = false;
  for (const auto &h : raw2->last_headers) {
    if (h == "Accept: application/vnd.github+json")
      has_accept = true;
  }
  assert(has_accept);

  return 0;
}
