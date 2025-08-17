#include "github_poller.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>
#include <unordered_map>

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

  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return "";
  }

private:
  std::atomic<int> &counter;
};

class BranchHttpClient : public HttpClient {
public:
  std::unordered_map<std::string, std::string> responses;
  std::string last_deleted;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    auto it = responses.find(url);
    if (it != responses.end()) {
      return it->second;
    }
    return "{}";
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
    (void)headers;
    last_deleted = url;
    return "";
  }
};

int main() {
  std::atomic<int> count1{0};
  auto http1 = std::make_unique<CountHttpClient>(count1);
  GitHubClient client1("tok", std::unique_ptr<HttpClient>(http1.release()));
  GitHubPoller poller1(client1, {{"me", "repo"}}, 50, 120);
  poller1.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(220));
  poller1.stop();
  assert(count1 >= 6); // multiple GETs per poll

  std::atomic<int> count2{0};
  auto http2 = std::make_unique<CountHttpClient>(count2);
  GitHubClient client2("tok", std::unique_ptr<HttpClient>(http2.release()));
  GitHubPoller poller2(client2, {{"me", "repo"}}, 50, 1);
  poller2.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(220));
  poller2.stop();
  assert(count2 == 2); // rate limited to first poll (two GETs)

  // Dirty branch is only warned when reject_dirty is false.
  {
    auto http = std::make_unique<BranchHttpClient>();
    BranchHttpClient *raw = http.get();
    std::string base = "https://api.github.com/repos/me/repo";
    raw->responses[base] = "{\"default_branch\":\"main\"}";
    raw->responses[base + "/branches"] =
        "[{\"name\":\"main\",\"commit\":{\"sha\":\"1\"}},{\"name\":\"feature\","
        "\"commit\":{\"sha\":\"2\"}}]";
    GitHubClient client("tok", std::unique_ptr<HttpClient>(http.release()));
    GitHubPoller poller(client, {{"me", "repo"}}, 100, 120, false, true, false);
    poller.poll_now();
    assert(raw->last_deleted.empty());
  }

  // Dirty branch is deleted when reject_dirty is true.
  {
    auto http = std::make_unique<BranchHttpClient>();
    BranchHttpClient *raw = http.get();
    std::string base = "https://api.github.com/repos/me/repo";
    raw->responses[base] = "{\"default_branch\":\"main\"}";
    raw->responses[base + "/branches"] =
        "[{\"name\":\"main\",\"commit\":{\"sha\":\"1\"}},{\"name\":\"feature\","
        "\"commit\":{\"sha\":\"2\"}}]";
    GitHubClient client("tok", std::unique_ptr<HttpClient>(http.release()));
    GitHubPoller poller(client, {{"me", "repo"}}, 100, 120, false, true, true);
    poller.poll_now();
    assert(raw->last_deleted == base + "/git/refs/heads/feature");
  }
  return 0;
}
