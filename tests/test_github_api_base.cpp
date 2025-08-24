#include "github_client.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

using namespace agpm;

class UrlHttpClient : public HttpClient {
public:
  std::string last_url;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    last_url = url;
    return "[]";
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
    (void)url;
    (void)headers;
    return "";
  }
};

TEST_CASE("github client uses custom api base") {
  auto http = std::make_unique<UrlHttpClient>();
  UrlHttpClient *raw = http.get();
  GitHubClient client({"tok"}, std::move(http), {}, {}, 0, 30000, 3,
                      "https://example.com");
  (void)client.list_repositories();
  REQUIRE(raw->last_url.rfind("https://example.com/", 0) == 0);
}
