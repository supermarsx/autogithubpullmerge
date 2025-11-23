#include "github_client.hpp"
#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <vector>

using namespace agpm;

class ConcurrentFake : public HttpClient {
 public:
  std::atomic<int> calls{0};
  std::string get(const std::string &url, const std::vector<std::string> &headers) override {
    (void)url; (void)headers;
    ++calls;
    // Simulate light work
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    return "[]";
  }
  std::string put(const std::string &, const std::string &, const std::vector<std::string> &) override { return "{}"; }
  std::string del(const std::string &, const std::vector<std::string> &) override { return ""; }
};

TEST_CASE("github client concurrent access") {
  auto http = std::make_unique<ConcurrentFake>();
  auto *raw = http.get();
  GitHubClient client({"token"}, std::move(http));

  const int nthreads = 8;
  const int iters = 25;
  std::vector<std::thread> threads;
  for (int t = 0; t < nthreads; ++t) {
    threads.emplace_back([&client, iters]() {
      for (int i = 0; i < iters; ++i) {
        client.list_pull_requests("me", "repo");
        client.list_branches_single("me/repo", 10);
      }
    });
  }
  for (auto &th : threads) th.join();

  REQUIRE(raw->calls.load() >= nthreads * iters);
}
