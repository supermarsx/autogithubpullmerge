#include "github_client.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <unordered_map>

using namespace agpm;

class ProtectCleanupHttpClient : public HttpClient {
public:
  std::string response;
  std::string last_deleted;
  std::string get(const std::string &,
                  const std::vector<std::string> &) override {
    return response;
  }
  HttpResponse get_with_headers(const std::string &,
                                const std::vector<std::string> &) override {
    HttpResponse r;
    r.status_code = 200;
    r.body = response;
    return r;
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

class ProtectBranchHttpClient : public HttpClient {
public:
  std::unordered_map<std::string, std::string> responses;
  std::string last_deleted;
  std::string get(const std::string &url,
                  const std::vector<std::string> &) override {
    return responses[url];
  }
  HttpResponse get_with_headers(const std::string &url,
                                const std::vector<std::string> &) override {
    HttpResponse r;
    r.status_code = 200;
    r.body = responses[url];
    return r;
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

TEST_CASE("branch protection excludes override patterns") {
  auto http = std::make_unique<ProtectCleanupHttpClient>();
  http->response = "[{\"head\":{\"ref\":\"tmp/safe\"}},"
                   "{\"head\":{\"ref\":\"tmp/remove\"}}]";
  ProtectCleanupHttpClient *raw = http.get();
  GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
  client.cleanup_branches("me", "repo", "tmp/", {"tmp/.*"}, {"tmp/remove"});
  REQUIRE(raw->last_deleted ==
          "https://api.github.com/repos/me/repo/git/refs/heads/tmp/remove");
}

TEST_CASE("dirty branches excluded from protection are purged") {
  auto http = std::make_unique<ProtectBranchHttpClient>();
  ProtectBranchHttpClient *raw = http.get();
  std::string base = "https://api.github.com/repos/me/repo";
  raw->responses[base] = "{\"default_branch\":\"main\"}";
  raw->responses[base + "/branches"] =
      "[{\"name\":\"main\"},{\"name\":\"tmp/safe\"},{\"name\":\"tmp/remove\"}]";
  raw->responses[base + "/compare/main...tmp/safe"] =
      "{\"status\":\"ahead\",\"ahead_by\":1}";
  raw->responses[base + "/compare/main...tmp/remove"] =
      "{\"status\":\"ahead\",\"ahead_by\":1}";
  GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
  client.close_dirty_branches("me", "repo", {"tmp/.*"}, {"tmp/remove"});
  REQUIRE(raw->last_deleted == base + "/git/refs/heads/tmp/remove");
}
