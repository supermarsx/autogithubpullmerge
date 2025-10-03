#include "poller.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

int main() {
  agpm::Poller poller(1, 1);
  poller.start();
  std::atomic<int> count{0};
  auto fut = poller.submit([&](){ ++count; });
  fut.wait();
  std::cout << "count after first: " << count.load() << "\n";
  auto fut2 = poller.submit([&](){ ++count; });
  std::this_thread::sleep_for(std::chrono::milliseconds(220));
  poller.stop();
  fut2.wait();
  std::cout << "count after second: " << count.load() << "\n";
  return 0;
}
