#include "poller.hpp"
#include <algorithm>
#include <memory>
#include <thread>

namespace agpm {

Poller::Poller(int workers, int max_rate)
    : workers_(std::max(1, workers)), max_rate_(max_rate) {
  if (max_rate_ > 0) {
    auto interval =
        std::chrono::duration<double>(60.0 / static_cast<double>(max_rate_));
    min_interval_ =
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            interval);
    if (min_interval_.count() <= 0) {
      min_interval_ = std::chrono::nanoseconds(1);
    }
  }
  next_allowed_ = std::chrono::steady_clock::now();
}

Poller::~Poller() { stop(); }

void Poller::start() {
  if (running_)
    return;
  running_ = true;
  next_allowed_ = std::chrono::steady_clock::now();
  threads_.reserve(workers_);
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

bool Poller::acquire_token() {
  if (max_rate_ <= 0)
    return running_;
  std::unique_lock<std::mutex> lock(rate_mutex_);
  while (running_) {
    auto now = std::chrono::steady_clock::now();
    if (now >= next_allowed_) {
      next_allowed_ = now + min_interval_;
      return true;
    }
    lock.unlock();
    auto wait = next_allowed_ - now;
    if (wait > std::chrono::milliseconds(50)) {
      wait = std::chrono::milliseconds(50);
    }
    std::this_thread::sleep_for(wait);
    lock.lock();
  }
  return false;
}

void Poller::worker() {
  while (true) {
    std::function<void()> job;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return !running_ || !jobs_.empty(); });
      if (!running_)
        return;
      job = std::move(jobs_.front());
      jobs_.pop();
    }
    if (!acquire_token()) {
      return;
    }
    job();
  }
}

} // namespace agpm
