#include "poller.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>

using namespace agpm;

int main() {
  std::atomic<int> count{0};
  Poller p([&] { ++count; }, 50, 120); // high rate
  p.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(220));
  p.stop();
  assert(count >= 3); // roughly 4 iterations expected

  count = 0;
  Poller p2([&] { ++count; }, 50, 1); // rate limited to 1/min
  p2.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(220));
  p2.stop();
  assert(count == 1); // first token only
  return 0;
}
