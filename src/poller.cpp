#include "poller.hpp"
#include <algorithm>
#include <memory>
#include <thread>

namespace agpm {

Poller::Poller(int workers, int max_rate)
    : workers_(workers), max_rate_(max_rate), tokens_(max_rate),
      capacity_(max_rate),
      fill_rate_(max_rate > 0 ? static_cast<double>(max_rate) / 60.0 : 0.0) {}

Poller::~Poller() { stop(); }

void Poller::start() {
  if (running_)
    return;
  running_ = true;
  last_fill_ = std::chrono::steady_clock::now();
  for (int i = 0; i < workers_; ++i) {
    threads_.emplace_back(&Poller::worker, this);
  }
}

void Poller::stop() {
  if (!running_)
    return;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
  }
  cv_.notify_all();
  for (auto &t : threads_) {
    if (t.joinable())
      t.join();
  }
  threads_.clear();
}

std::future<void> Poller::submit(std::function<void()> job) {
  if (!running_) {
    std::packaged_task<void()> pt(std::move(job));
    auto fut = pt.get_future();
    pt();
    return fut;
  }
  auto task = std::make_shared<std::packaged_task<void()>>(std::move(job));
  std::future<void> fut = task->get_future();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    jobs_.emplace([task]() { (*task)(); });
  }
  cv_.notify_one();
  return fut;
}

void Poller::acquire_token() {
  if (max_rate_ <= 0)
    return;
  std::unique_lock<std::mutex> lock(rate_mutex_);
  while (running_) {
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - last_fill_).count();
    tokens_ = std::min(capacity_, tokens_ + elapsed * fill_rate_);
    last_fill_ = now;
    if (tokens_ >= 1.0) {
      tokens_ -= 1.0;
      return;
    }
    lock.unlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    lock.lock();
  }
}

void Poller::worker() {
  while (true) {
    std::function<void()> job;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return !running_ || !jobs_.empty(); });
      if (!running_ && jobs_.empty())
        return;
      job = std::move(jobs_.front());
      jobs_.pop();
    }
    acquire_token();
    job();
  }
}

} // namespace agpm

