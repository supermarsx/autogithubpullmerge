#include "github_client.hpp"
#include <catch2/catch_test_macros.hpp>
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
  std::string last_url;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    last_url = url;
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

class PagingCleanupHttpClient : public HttpClient {
public:
  int page = 0;
  std::string last_deleted;
  std::string get(const std::string &,
                  const std::vector<std::string> &) override {
    return "";
  }
  HttpResponse
  get_with_headers(const std::string &url,
                   const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    HttpResponse res;
    res.status_code = 200;
    if (page == 0) {
      res.body = "[{\"head\":{\"ref\":\"keep\"}}]";
      res.headers = {"Link: "
                     "<https://api.github.com/repos/me/repo/"
                     "pulls?state=closed&page=2>; rel=\"next\""};
      page++;
    } else {
      res.body = "[{\"head\":{\"ref\":\"tmp/paged\"}}]";
    }
    return res;
  }
  std::string put(const std::string &, const std::string &,
                  const std::vector<std::string> &) override {
    return "{}";
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &) override {
    last_deleted = url;
    return "";
  }
};

class PagingBranchHttpClient : public HttpClient {
public:
  std::string base = "https://api.github.com/repos/me/repo";
  std::string last_deleted;
  std::string get(const std::string &url,
                  const std::vector<std::string> &) override {
    if (url == base)
      return "{\"default_branch\":\"main\"}";
    if (url == base + "/compare/main...feature1")
      return "{\"status\":\"identical\"}";
    if (url == base + "/compare/main...feature2")
      return "{\"status\":\"ahead\",\"ahead_by\":1}";
    return "{}";
  }
  HttpResponse get_with_headers(const std::string &url,
                                const std::vector<std::string> &) override {
    HttpResponse res;
    res.status_code = 200;
    if (url == base + "/branches") {
      res.body = "[{\"name\":\"main\"},{\"name\":\"feature1\"}]";
      res.headers = {"Link: <" + base + "/branches?page=2>; rel=\"next\""};
    } else {
      res.body = "[{\"name\":\"feature2\"}]";
    }
    return res;
  }
  std::string put(const std::string &, const std::string &,
                  const std::vector<std::string> &) override {
    return "{}";
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &) override {
    last_deleted = url;
    return "";
  }
};

TEST_CASE("test branch cleanup") {
  // Purge branches based on prefix.
  {
    auto http = std::make_unique<CleanupHttpClient>();
    http->response =
        "[{\"head\":{\"ref\":\"tmp/feature\"}},{\"head\":{\"ref\":\"keep\"}}]";
    CleanupHttpClient *raw = http.get();
    GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
    client.cleanup_branches("me", "repo", "tmp/");
    REQUIRE(raw->last_url.find("state=closed") != std::string::npos);
    REQUIRE(raw->last_deleted ==
            "https://api.github.com/repos/me/repo/git/refs/heads/tmp/feature");
  }

  // Protected branch matching pattern should not be deleted.
  {
    auto http = std::make_unique<CleanupHttpClient>();
    http->response = "[{\"head\":{\"ref\":\"tmp/protected\"}}]";
    CleanupHttpClient *raw = http.get();
    GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
    client.cleanup_branches("me", "repo", "tmp/", {"tmp/*"});
    REQUIRE(raw->last_deleted.empty());
  }

  // Clean branch should not be deleted.
  {
    auto http = std::make_unique<BranchHttpClient>();
    BranchHttpClient *raw = http.get();
    std::string base = "https://api.github.com/repos/me/repo";
    raw->responses[base] = "{\"default_branch\":\"main\"}";
    raw->responses[base + "/branches"] =
        "[{\"name\":\"main\"},{\"name\":\"feature\"}]";
    raw->responses[base + "/compare/main...feature"] =
        "{\"status\":\"identical\"}";
    GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
    client.close_dirty_branches("me", "repo");
    REQUIRE(raw->last_deleted.empty());
  }

  // Dirty branch should be deleted.
  {
    auto http = std::make_unique<BranchHttpClient>();
    BranchHttpClient *raw = http.get();
    std::string base = "https://api.github.com/repos/me/repo";
    raw->responses[base] = "{\"default_branch\":\"main\"}";
    raw->responses[base + "/branches"] =
        "[{\"name\":\"main\"},{\"name\":\"feature\"}]";
    raw->responses[base + "/compare/main...feature"] =
        "{\"status\":\"ahead\",\"ahead_by\":1}";
    GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
    client.close_dirty_branches("me", "repo");
    REQUIRE(raw->last_deleted == base + "/git/refs/heads/feature");
  }

  // Dirty branch matching protected pattern should remain.
  {
    auto http = std::make_unique<BranchHttpClient>();
    BranchHttpClient *raw = http.get();
    std::string base = "https://api.github.com/repos/me/repo";
    raw->responses[base] = "{\"default_branch\":\"main\"}";
    raw->responses[base + "/branches"] =
        "[{\"name\":\"main\"},{\"name\":\"feature\"}]";
    raw->responses[base + "/compare/main...feature"] =
        "{\"status\":\"ahead\",\"ahead_by\":1}";
    GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
    client.close_dirty_branches("me", "repo", {"feat*"});
    REQUIRE(raw->last_deleted.empty());
  }

  // Purge branches across paginated pull request pages.
  {
    auto http = std::make_unique<PagingCleanupHttpClient>();
    const PagingCleanupHttpClient *raw = http.get();
    GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
    client.cleanup_branches("me", "repo", "tmp/");
    REQUIRE(raw->last_deleted ==
            "https://api.github.com/repos/me/repo/git/refs/heads/tmp/paged");
  }

  // Dirty branch discovered on later page should be deleted.
  {
    auto http = std::make_unique<PagingBranchHttpClient>();
    const PagingBranchHttpClient *raw = http.get();
    GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
    client.close_dirty_branches("me", "repo");
    REQUIRE(raw->last_deleted == raw->base + "/git/refs/heads/feature2");
  }
}
