#include "github_client.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>

using namespace agpm;

class SpyHttpClient : public HttpClient {
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

TEST_CASE("test github filter") {
  auto http = std::make_unique<SpyHttpClient>();
  http->response = "[{\"number\":1,\"title\":\"Test\"}]";
  SpyHttpClient *raw1 = http.get();
  GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()),
                      {"me/allowed"}, {"me/skip"});

  // not allowed by include filter
  auto prs = client.list_pull_requests("me", "other");
  REQUIRE(prs.empty());
  REQUIRE(raw1->last_method.empty());

  // allowed repository
  auto http2 = std::make_unique<SpyHttpClient>();
  http2->response = "[{\"number\":2,\"title\":\"Good\"}]";
  GitHubClient client2({"tok"}, std::unique_ptr<HttpClient>(http2.release()),
                       {"me/good"}, {});
  auto prs2 = client2.list_pull_requests("me", "good");
  REQUIRE(prs2.size() == 1);

  // excluded repository
  auto http3 = std::make_unique<SpyHttpClient>();
  SpyHttpClient *raw3 = http3.get();
  GitHubClient client3({"tok"}, std::unique_ptr<HttpClient>(http3.release()),
                       {}, {"me/bad"});
  bool merged = client3.merge_pull_request("me", "bad", 1);
  REQUIRE(!merged);
  REQUIRE(raw3->last_method.empty());
}
