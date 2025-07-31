#include "poller.hpp"
#include <algorithm>
#include <thread>

namespace agpm {

Poller::Poller(std::function<void()> task, int interval_ms, int max_rate)
    : task_(std::move(task)), interval_ms_(interval_ms), max_rate_(max_rate),
      tokens_(max_rate), capacity_(max_rate),
      fill_rate_(static_cast<double>(max_rate) / 60.0) {}

Poller::~Poller() { stop(); }

void Poller::start() {
  if (running_) {
    return;
  }
  running_ = true;
  last_fill_ = std::chrono::steady_clock::now();
  thread_ = std::thread(&Poller::run, this);
}

void Poller::stop() {
  if (!running_) {
    return;
  }
  running_ = false;
  if (thread_.joinable()) {
    thread_.join();
  }
}

void Poller::run() {
  while (running_) {
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - last_fill_).count();
    tokens_ = std::min(capacity_, tokens_ + elapsed * fill_rate_);
    last_fill_ = now;
    if (tokens_ >= 1.0) {
      tokens_ -= 1.0;
      task_();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));
  }
}

} // namespace agpm
