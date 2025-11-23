#include "github_client.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace agpm;

class ThrowTransient : public HttpClient {
public:
  int calls = 0;
  std::string get(const std::string &,
                  const std::vector<std::string> &) override {
    if (calls++ == 0)
      throw TransientNetworkError("transient");
    return "[]";
  }
  std::string put(const std::string &, const std::string &,
                  const std::vector<std::string> &) override {
    return "";
  }
  std::string del(const std::string &,
                  const std::vector<std::string> &) override {
    return "";
  }
};

class ThrowHttp500 : public HttpClient {
public:
  int calls = 0;
  std::string get(const std::string &,
                  const std::vector<std::string> &) override {
    if (calls++ == 0)
      throw HttpStatusError(502, "server");
    return "[]";
  }
  std::string put(const std::string &, const std::string &,
                  const std::vector<std::string> &) override {
    return "";
  }
  std::string del(const std::string &,
                  const std::vector<std::string> &) override {
    return "";
  }
};

TEST_CASE("retry typed errors") {
  {
    auto http = std::make_unique<ThrowTransient>();
    auto *raw = http.get();
    GitHubClient client({"tok"}, std::move(http));
    auto res = client.list_open_pull_requests_single("me/repo");
    REQUIRE(raw->calls == 2);
  }
  {
    auto http = std::make_unique<ThrowHttp500>();
    auto *raw = http.get();
    GitHubClient client({"tok"}, std::move(http));
    auto res = client.list_open_pull_requests_single("me/repo");
    REQUIRE(raw->calls == 2);
  }
}
