#include "github_client.hpp"
#include <cassert>
#include <string>

using namespace agpm;

class CleanupHttpClient : public HttpClient {
public:
  std::string response;
  std::string last_deleted;
  std::string last_url;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    last_url = url;
    return response;
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

int main() {
  auto http = std::make_unique<CleanupHttpClient>();
  http->response =
      "[{\"head\":{\"ref\":\"tmp/feature\"}},{\"head\":{\"ref\":\"keep\"}}]";
  CleanupHttpClient *raw = http.get();
  GitHubClient client("tok", std::unique_ptr<HttpClient>(http.release()));
  client.cleanup_branches("me", "repo", "tmp/");
  assert(raw->last_url.find("state=closed") != std::string::npos);
  assert(raw->last_deleted ==
         "https://api.github.com/repos/me/repo/git/refs/heads/tmp/feature");
  return 0;
}
