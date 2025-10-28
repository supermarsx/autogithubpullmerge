#include "github_client.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <ctime>
#include <string>

using namespace agpm;

class TimeHttpClient : public HttpClient {
public:
  std::string response;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
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
    (void)url;
    (void)headers;
    return "";
  }
};

TEST_CASE("test pr since") {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto recent_update = now - minutes(30);
  auto old = now - hours(5);
  auto recent_update_t = system_clock::to_time_t(recent_update);
  auto old_t = system_clock::to_time_t(old);
  char recent_update_buf[32];
  char old_buf[32];
  std::strftime(recent_update_buf, sizeof(recent_update_buf),
                "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&recent_update_t));
  std::strftime(old_buf, sizeof(old_buf), "%Y-%m-%dT%H:%M:%SZ",
                std::gmtime(&old_t));
  std::string resp =
      std::string("[{\"number\":1,\"title\":\"Old\",\"created_at\":\"") +
      old_buf +
      "\"},{\"number\":2,\"title\":\"Reopened\",\"created_at\":\"" +
      old_buf + "\",\"updated_at\":\"" + recent_update_buf + "\"}]";
  auto http = std::make_unique<TimeHttpClient>();
  http->response = resp;
  GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
  auto prs = client.list_pull_requests("me", "repo", false, 50, hours(1));
  REQUIRE(prs.size() == 1);
  REQUIRE(prs[0].number == 2);
}
