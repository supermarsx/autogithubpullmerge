#include "github_poller.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>

using namespace agpm;

class MergeHttpClient : public HttpClient {
public:
  MergeHttpClient() : merge_calls(0) {}
  std::atomic<int> merge_calls;
  std::string last_url;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    if (url.find("/pulls") != std::string::npos) {
      return "[{\"number\":1,\"title\":\"PR\"}]";
    }
    return "[]";
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)data;
    (void)headers;
    last_url = url;
    merge_calls++;
    return "{\"merged\":true}";
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return "";
  }
};

int main() {
  auto http = std::make_unique<MergeHttpClient>();
  MergeHttpClient *raw = http.get();
  GitHubClient client("tok", std::unique_ptr<HttpClient>(http.release()));
  GitHubPoller poller(client, {{"me", "repo"}}, 50, 120, false, false, false,
                      "", true);
  poller.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  poller.stop();
  assert(raw->merge_calls > 0);
  assert(raw->last_url.find("/repos/me/repo/pulls/1/merge") !=
         std::string::npos);
  return 0;
}
