#include "github_poller.hpp"
#include <cassert>
#include <string>
#include <unordered_set>

using namespace agpm;

class BranchListClient : public HttpClient {
public:
  int branch_get_count = 0;
  std::string last_deleted;
  std::string base = "https://api.github.com/repos/me/repo";
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    if (url == base)
      return "{\"default_branch\":\"main\"}";
    if (url == base + "/branches") {
      branch_get_count++;
      return "[{\"name\":\"main\"},{\"name\":\"feature\"}]";
    }
    return "[]";
  }
  HttpResponse
  get_with_headers(const std::string &url,
                   const std::vector<std::string> &headers) override {
    HttpResponse res;
    res.status_code = 200;
    res.body = get(url, headers);
    return res;
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

class BranchCleanupClient : public HttpClient {
public:
  std::string base = "https://api.github.com/repos/me/repo";
  std::unordered_set<std::string> deleted;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    if (url == base)
      return "{\"default_branch\":\"main\"}";
    if (url == base + "/branches")
      return "[{\"name\":\"main\"},{\"name\":\"tmp/"
             "purge\"},{\"name\":\"feature\"}]";
    if (url == base + "/compare/main...feature")
      return "{\"status\":\"ahead\",\"ahead_by\":1}";
    if (url == base + "/compare/main...tmp/purge")
      return "{\"status\":\"identical\"}";
    if (url == base + "/pulls?state=closed")
      return "[{\"head\":{\"ref\":\"tmp/purge\"}}]";
    return "[]";
  }
  HttpResponse
  get_with_headers(const std::string &url,
                   const std::vector<std::string> &headers) override {
    HttpResponse res;
    res.status_code = 200;
    res.body = get(url, headers);
    return res;
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
    deleted.insert(url);
    return "";
  }
};

int main() {
  // Detect stray branches without cleanup
  {
    auto http = std::make_unique<BranchListClient>();
    BranchListClient *raw = http.get();
    GitHubClient client("tok", std::unique_ptr<HttpClient>(http.release()));
    GitHubPoller poller(client, {{"me", "repo"}}, 1000, 60, false, true);
    std::string msg;
    poller.set_log_callback([&](const std::string &m) { msg = m; });
    poller.poll_now();
    assert(raw->branch_get_count == 1);
    assert(msg.find("stray branches: 1") != std::string::npos);
    assert(raw->last_deleted.empty());
  }

  // Cleanup branches and close dirty ones
  {
    auto http = std::make_unique<BranchCleanupClient>();
    BranchCleanupClient *raw = http.get();
    GitHubClient client("tok", std::unique_ptr<HttpClient>(http.release()));
    GitHubPoller poller(client, {{"me", "repo"}}, 1000, 60, false, true, true,
                        "tmp/");
    poller.poll_now();
    assert(raw->deleted.count(raw->base + "/git/refs/heads/feature") == 1);
    assert(raw->deleted.count(raw->base + "/git/refs/heads/tmp/purge") == 1);
  }

  return 0;
}
