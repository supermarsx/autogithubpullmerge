#include "github_client.hpp"
#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <string>

using namespace agpm;

class FlakyHttpClient : public HttpClient {
public:
  int calls = 0;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    if (calls++ < 2) {
      throw HttpStatusError(500, "curl GET failed with HTTP code 500");
    }
    return "[{\"number\":1,\"title\":\"PR\"}]";
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    (void)headers;
    return "";
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return "";
  }
};

class BadRequestHttpClient : public HttpClient {
public:
  int calls = 0;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    ++calls;
    throw HttpStatusError(400, "curl GET failed with HTTP code 400");
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    (void)headers;
    return "";
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return "";
  }
};

TEST_CASE("test github client retry") {
  {
    auto http = std::make_unique<FlakyHttpClient>();
    auto *raw = http.get();
    GitHubClient client({"token"}, std::move(http));
    auto prs = client.list_pull_requests("o", "r");
    REQUIRE(prs.size() == 1);
    REQUIRE(raw->calls == 3);
  }
  {
    auto http = std::make_unique<BadRequestHttpClient>();
    auto *raw = http.get();
    GitHubClient client({"token"}, std::move(http));
    auto prs = client.list_pull_requests("o", "r");
    REQUIRE(prs.empty());
    REQUIRE(raw->calls == 1);
  }
}
