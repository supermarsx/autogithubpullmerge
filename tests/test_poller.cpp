#include "poller.hpp"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

using namespace agpm;

TEST_CASE("thread pool runs tasks concurrently") {
  Poller p(2, 0); // no rate limiting
  p.start();
  std::atomic<int> count{0};
  auto start = std::chrono::steady_clock::now();
  auto f1 = p.submit([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ++count;
  });
  auto f2 = p.submit([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ++count;
  });
  f1.get();
  f2.get();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - start)
                     .count();
  p.stop();
  REQUIRE(count == 2);
  REQUIRE(elapsed < 180);
}

TEST_CASE("thread pool handles more tasks than workers") {
  Poller p(2, 0);
  p.start();
  std::atomic<int> count{0};
  auto start = std::chrono::steady_clock::now();
  std::vector<std::future<void>> futs;
  for (int i = 0; i < 4; ++i) {
    futs.push_back(p.submit([&] {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      ++count;
    }));
  }
  for (auto &f : futs) {
    f.get();
  }
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - start)
                     .count();
  p.stop();
  REQUIRE(count == 4);
  REQUIRE(elapsed < 360);
}
