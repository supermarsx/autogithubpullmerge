#include "github_poller.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>

using namespace agpm;

class CountHttpClient : public HttpClient {
public:
  explicit CountHttpClient(std::atomic<int> &c) : counter(c) {}
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    ++counter;
    return "[]";
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    (void)headers;
    return "{}";
  }

private:
  std::atomic<int> &counter;
};

int main() {
  std::atomic<int> count1{0};
  auto http1 = std::make_unique<CountHttpClient>(count1);
  GitHubClient client1("tok", std::unique_ptr<HttpClient>(http1.release()));
  GitHubPoller poller1(client1, {{"me", "repo"}}, 50, 120);
  poller1.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(220));
  poller1.stop();
  assert(count1 >= 3); // should run ~4 times

  std::atomic<int> count2{0};
  auto http2 = std::make_unique<CountHttpClient>(count2);
  GitHubClient client2("tok", std::unique_ptr<HttpClient>(http2.release()));
  GitHubPoller poller2(client2, {{"me", "repo"}}, 50, 1);
  poller2.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(220));
  poller2.stop();
  assert(count2 == 1); // rate limited to first token
  return 0;
}
