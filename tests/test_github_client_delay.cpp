#include "github_client.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdlib>
#include <curl/curl.h>
#include <optional>
#include <string>

using namespace agpm;

namespace {

class ScopedUnsetEnv {
public:
  explicit ScopedUnsetEnv(const char *name) : name_(name) {
    const char *existing = std::getenv(name);
    if (existing) {
      value_ = existing;
    }
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
  }

  ScopedUnsetEnv(const ScopedUnsetEnv &) = delete;
  ScopedUnsetEnv &operator=(const ScopedUnsetEnv &) = delete;

  ~ScopedUnsetEnv() {
#ifdef _WIN32
    if (value_) {
      _putenv_s(name_.c_str(), value_->c_str());
    } else {
      _putenv_s(name_.c_str(), "");
    }
#else
    if (value_) {
      setenv(name_.c_str(), value_->c_str(), 1);
    } else {
      unsetenv(name_.c_str());
    }
#endif
  }

private:
  std::string name_;
  std::optional<std::string> value_;
};

} // namespace

class DelayHttpClient : public HttpClient {
public:
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    if (url.find("/pulls/") != std::string::npos) {
      return "{}";
    }
    return "[]";
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    (void)headers;
    return "{\"merged\":true}";
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return "";
  }
};

TEST_CASE("test github client delay") {
  auto http = std::make_unique<DelayHttpClient>();
  DelayHttpClient *raw = http.get();
  (void)raw;
  GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()), {},
                      {}, 100);
  client.list_pull_requests("owner", "repo");
  auto t2 = std::chrono::steady_clock::now();
  client.list_pull_requests("owner", "repo");
  auto t3 = std::chrono::steady_clock::now();
  auto diff =
      std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();
  REQUIRE(diff >= 100);

  client.set_delay_ms(50);
  client.merge_pull_request("owner", "repo", 1);
  auto t5 = std::chrono::steady_clock::now();
  client.merge_pull_request("owner", "repo", 1);
  auto t6 = std::chrono::steady_clock::now();
  diff = std::chrono::duration_cast<std::chrono::milliseconds>(t6 - t5).count();
  REQUIRE(diff >= 50);

  try {
    ScopedUnsetEnv unset_http("http_proxy");
    ScopedUnsetEnv unset_http_upper("HTTP_PROXY");
    ScopedUnsetEnv unset_https("https_proxy");
    ScopedUnsetEnv unset_https_upper("HTTPS_PROXY");
    CurlHttpClient real;
    real.get("http://192.0.2.1/", {});
    FAIL("Expected exception");
  } catch (const std::exception &e) {
    std::string msg = e.what();
    REQUIRE(msg.find("192.0.2.1") != std::string::npos);
    REQUIRE(msg.find(curl_easy_strerror(CURLE_COULDNT_CONNECT)) !=
            std::string::npos);
  }
}
