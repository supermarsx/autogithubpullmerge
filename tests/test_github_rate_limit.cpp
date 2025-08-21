#include "github_client.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <ctime>
#include <string>
#include <vector>

using namespace agpm;

class ResetHttpClient : public HttpClient {
public:
  int calls = 0;
  HttpResponse get_with_headers(const std::string &,
                                const std::vector<std::string> &) override {
    ++calls;
    if (calls == 1) {
      long reset = std::time(nullptr) + 2;
      return {"",
              {"X-RateLimit-Remaining: 0",
               "X-RateLimit-Reset: " + std::to_string(reset)},
              403};
    }
    return {"[]", {}, 200};
  }
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return ""; // unused
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

class RetryAfterHttpClient : public HttpClient {
public:
  int calls = 0;
  HttpResponse get_with_headers(const std::string &,
                                const std::vector<std::string> &) override {
    ++calls;
    if (calls == 1) {
      return {"", {"Retry-After: 1"}, 429};
    }
    return {"[]", {}, 200};
  }
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return "";
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

TEST_CASE("test github rate limit") {
  {
    auto http = std::make_unique<ResetHttpClient>();
    auto *raw = http.get();
    GitHubClient client("tok", std::move(http));
    auto start = std::chrono::steady_clock::now();
    client.list_pull_requests("o", "r");
    auto end = std::chrono::steady_clock::now();
    auto diff =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    REQUIRE(diff >= 1000);
    REQUIRE(raw->calls == 2);
  }
  {
    auto http = std::make_unique<RetryAfterHttpClient>();
    auto *raw = http.get();
    GitHubClient client("tok", std::move(http));
    auto start = std::chrono::steady_clock::now();
    client.list_pull_requests("o", "r");
    auto end = std::chrono::steady_clock::now();
    auto diff =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    REQUIRE(diff >= 1000);
    REQUIRE(raw->calls == 2);
  }
}
