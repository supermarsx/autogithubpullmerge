#include "github_client.hpp"
#include <cassert>
#include <string>
#include <unordered_map>

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

class BranchHttpClient : public HttpClient {
public:
  std::unordered_map<std::string, std::string> responses;
  std::string last_deleted;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
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

int main() {
  // Purge branches based on prefix.
  {
    auto http = std::make_unique<CleanupHttpClient>();
    http->response =
        "[{\"head\":{\"ref\":\"tmp/feature\"}},{\"head\":{\"ref\":\"keep\"}}]";
    CleanupHttpClient *raw = http.get();
    GitHubClient client("tok", std::unique_ptr<HttpClient>(http.release()));
    client.cleanup_branches("me", "repo", "tmp/");
    assert(raw->last_url.find("state=closed") != std::string::npos);
    assert(raw->last_deleted ==
           "https://api.github.com/repos/me/repo/git/refs/heads/tmp/feature");
  }

  // Clean branch should not be deleted.
  {
    auto http = std::make_unique<BranchHttpClient>();
    BranchHttpClient *raw = http.get();
    std::string base = "https://api.github.com/repos/me/repo";
    raw->responses[base] = "{\"default_branch\":\"main\"}";
    raw->responses[base + "/branches"] =
        "[{\"name\":\"main\",\"commit\":{\"sha\":\"1\"}},{\"name\":\"feature\","
        "\"commit\":{\"sha\":\"1\"}}]";
    GitHubClient client("tok", std::unique_ptr<HttpClient>(http.release()));
    client.close_dirty_branches("me", "repo", true);
    assert(raw->last_deleted.empty());
  }

  // Dirty branch should be deleted.
  {
    auto http = std::make_unique<BranchHttpClient>();
    BranchHttpClient *raw = http.get();
    std::string base = "https://api.github.com/repos/me/repo";
    raw->responses[base] = "{\"default_branch\":\"main\"}";
    raw->responses[base + "/branches"] =
        "[{\"name\":\"main\",\"commit\":{\"sha\":\"1\"}},{\"name\":\"feature\","
        "\"commit\":{\"sha\":\"2\"}}]";
    GitHubClient client("tok", std::unique_ptr<HttpClient>(http.release()));
    client.close_dirty_branches("me", "repo", true);
    assert(raw->last_deleted == base + "/git/refs/heads/feature");
  }

  return 0;
}
