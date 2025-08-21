#include "github_client.hpp"
#include <catch2/catch_test_macros.hpp>
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
    last_headers.push_back("User-Agent: autogithubpullmerge");
    return response;
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    last_headers = headers;
    last_headers.push_back("User-Agent: autogithubpullmerge");
    return response;
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    last_headers = headers;
    last_headers.push_back("User-Agent: autogithubpullmerge");
    return response;
  }
};

TEST_CASE("test github client headers") {
  auto http = std::make_unique<HeaderHttpClient>();
  http->response = "[]";
  HeaderHttpClient *raw = http.get();
  GitHubClient client("token123", std::unique_ptr<HttpClient>(http.release()));
  client.list_pull_requests("owner", "repo");
  bool found_auth = false;
  bool found_agent = false;
  for (const auto &h : raw->last_headers) {
    if (h == "Authorization: token token123")
      found_auth = true;
    if (h == "User-Agent: autogithubpullmerge")
      found_agent = true;
  }
  REQUIRE(found_auth && found_agent);

  auto http2 = std::make_unique<HeaderHttpClient>();
  http2->response = "{\"merged\":true}";
  HeaderHttpClient *raw2 = http2.get();
  GitHubClient client2("tok", std::unique_ptr<HttpClient>(http2.release()));
  bool merged = client2.merge_pull_request("owner", "repo", 1);
  REQUIRE(merged);
  found_auth = false;
  found_agent = false;
  for (const auto &h : raw2->last_headers) {
    if (h == "Authorization: token tok")
      found_auth = true;
    if (h == "User-Agent: autogithubpullmerge")
      found_agent = true;
  }
  REQUIRE(found_auth && found_agent);
}
