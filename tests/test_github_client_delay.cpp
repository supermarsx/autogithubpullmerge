#include "github_client.hpp"
#include <cassert>
#include <chrono>
#include <string>

using namespace agpm;

class DelayHttpClient : public HttpClient {
public:
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return "[]";
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    (void)headers;
    return "{\"merged\":true}";
  }
};

int main() {
  auto http = std::make_unique<DelayHttpClient>();
  DelayHttpClient *raw = http.get();
  (void)raw;
  GitHubClient client("tok", std::unique_ptr<HttpClient>(http.release()), {},
                      {}, 100);
  client.list_pull_requests("owner", "repo");
  auto t2 = std::chrono::steady_clock::now();
  client.list_pull_requests("owner", "repo");
  auto t3 = std::chrono::steady_clock::now();
  auto diff =
      std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();
  assert(diff >= 100);

  client.set_delay_ms(50);
  client.merge_pull_request("owner", "repo", 1);
  auto t5 = std::chrono::steady_clock::now();
  client.merge_pull_request("owner", "repo", 1);
  auto t6 = std::chrono::steady_clock::now();
  diff = std::chrono::duration_cast<std::chrono::milliseconds>(t6 - t5).count();
  assert(diff >= 50);

  return 0;
}
