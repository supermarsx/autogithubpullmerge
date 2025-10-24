#include "poller.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <thread>

namespace agpm {

/**
 * Construct a worker pool with optional rate limiting.
 *
 * @param workers Number of worker threads requested.
 * @param max_rate Maximum allowed requests per minute (0 = unlimited).
 * @param smoothing_factor Exponential moving-average factor used when
 *        estimating observed throughput.
 */
Poller::Poller(int workers, int max_rate, double smoothing_factor)
    : workers_(std::max(1, workers)), max_rate_(max_rate),
      smoothing_factor_(std::clamp(smoothing_factor, 0.01, 1.0)),
      last_execution_(std::chrono::steady_clock::time_point::min()) {
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
  update_queue_margin();
  next_allowed_ = std::chrono::steady_clock::now();
  queued_.store(0, std::memory_order_relaxed);
  in_flight_.store(0, std::memory_order_relaxed);
}

/**
 * Stop the worker pool on destruction.
 */
Poller::~Poller() { stop(); }

/**
 * Start worker threads if not already running.
 */
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

/**
 * Stop worker threads and clear pending jobs.
 */
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

/**
 * Submit a job to the worker queue.
 *
 * @param job Callable to execute either immediately (when the pool is stopped)
 *        or via a worker thread.
 * @return Future that is fulfilled when the job completes.
 */
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
    queued_.fetch_add(1, std::memory_order_relaxed);
  }
  cv_.notify_one();
  check_backlog();
  return fut;
}

/**
 * Adjust the token-bucket rate limit applied to queued jobs.
 *
 * @param max_rate New requests-per-minute ceiling (0 disables rate limiting).
 */
void Poller::set_max_rate(int max_rate) {
  std::lock_guard<std::mutex> lock(rate_mutex_);
  max_rate_ = max_rate;
  if (max_rate_ > 0) {
    auto interval =
        std::chrono::duration<double>(60.0 / static_cast<double>(max_rate_));
    min_interval_ =
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            interval);
    if (min_interval_.count() <= 0) {
      min_interval_ = std::chrono::nanoseconds(1);
    }
  } else {
    min_interval_ = std::chrono::steady_clock::duration::zero();
  }
  update_queue_margin();
  next_allowed_ = std::chrono::steady_clock::now();
}

/**
 * Update the exponential smoothing factor applied to throughput sampling.
 *
 * @param factor Value in the range (0, 1] weighting the latest execution.
 */
void Poller::set_smoothing_factor(double factor) {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  smoothing_factor_ = std::clamp(factor, 0.01, 1.0);
}

/**
 * Retrieve the exponentially smoothed requests-per-minute estimate.
 *
 * @return Moving-average request rate reflecting recent execution cadence.
 */
double Poller::smoothed_requests_per_minute() const {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  double rpm = ema_rpm_;
  if (rpm <= 0.0 && max_rate_ > 0) {
    rpm = static_cast<double>(max_rate_);
  }
  return rpm;
}

/**
 * Return the number of queued and in-flight jobs managed by the scheduler.
 *
 * @return Outstanding job count awaiting processing or completion.
 */
std::size_t Poller::outstanding_jobs() const {
  return queued_.load(std::memory_order_relaxed) +
         in_flight_.load(std::memory_order_relaxed);
}

/**
 * Estimate the time required to drain the current backlog.
 *
 * @return Clearance estimate rounded to seconds, or `std::nullopt` when the
 *         scheduler lacks sufficient data to compute a forecast.
 */
std::optional<std::chrono::seconds> Poller::estimate_clearance_time() const {
  auto outstanding = outstanding_jobs();
  if (outstanding == 0) {
    return std::chrono::seconds(0);
  }
  return estimate_clearance_time_unlocked(outstanding);
}

/**
 * Configure backlog alert thresholds and notification callback.
 *
 * @param job_threshold Minimum outstanding jobs required to emit an alert.
 * @param clearance_threshold Minimum estimated clearance time before alerting.
 * @param cb Callback invoked with the backlog size and clearance estimate.
 */
void Poller::set_backlog_alert(
    std::size_t job_threshold, std::chrono::seconds clearance_threshold,
    std::function<void(std::size_t, std::chrono::seconds)> cb) {
  backlog_job_threshold_ = job_threshold;
  backlog_time_threshold_ = clearance_threshold;
  backlog_callback_ = std::move(cb);
  last_backlog_alert_ = std::chrono::steady_clock::time_point::min();
}

/**
 * Enforce the configured rate limit before executing a job.
 *
 * @return `true` if execution may proceed, `false` when the pool is stopping.
 */
bool Poller::acquire_token() {
  if (max_rate_ <= 0)
    return running_;
  std::unique_lock<std::mutex> lock(rate_mutex_);
  while (running_) {
    auto now = std::chrono::steady_clock::now();
    if (now >= next_allowed_) {
      if (min_interval_ <= std::chrono::steady_clock::duration::zero()) {
        next_allowed_ = now;
        return true;
      }
      auto scheduled_next = next_allowed_ + min_interval_;
      auto margin = queue_margin_;
      if (margin > min_interval_) {
        margin = min_interval_;
      }
      auto earliest_next = now + min_interval_ - margin;
      if (earliest_next < now) {
        earliest_next = now;
      }
      if (scheduled_next < earliest_next) {
        next_allowed_ = earliest_next;
      } else {
        next_allowed_ = scheduled_next;
      }
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

void Poller::update_queue_margin() {
  if (min_interval_ <= std::chrono::steady_clock::duration::zero()) {
    queue_margin_ = std::chrono::steady_clock::duration::zero();
    return;
  }
  auto min_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(min_interval_);
  long double scaled = static_cast<long double>(min_ns.count()) *
                       static_cast<long double>(queue_balance_slack_);
  auto margin_ns = static_cast<long long>(std::llround(scaled));
  if (margin_ns <= 0) {
    queue_margin_ = std::chrono::steady_clock::duration::zero();
    return;
  }
  auto margin = std::chrono::nanoseconds(margin_ns);
  if (margin >= min_ns) {
    if (min_ns.count() <= 1) {
      queue_margin_ = std::chrono::steady_clock::duration::zero();
      return;
    }
    margin = min_ns - std::chrono::nanoseconds(1);
  }
  queue_margin_ =
      std::chrono::duration_cast<std::chrono::steady_clock::duration>(margin);
}

/**
 * Worker thread loop processing queued jobs.
 */
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
      queued_.fetch_sub(1, std::memory_order_relaxed);
      in_flight_.fetch_add(1, std::memory_order_relaxed);
    }
    if (!acquire_token()) {
      in_flight_.fetch_sub(1, std::memory_order_relaxed);
      return;
    }
    job();
    in_flight_.fetch_sub(1, std::memory_order_relaxed);
    record_execution();
    check_backlog();
  }
}

void Poller::record_execution() {
  auto now = std::chrono::steady_clock::now();
  std::lock_guard<std::mutex> lock(stats_mutex_);
  if (last_execution_ == std::chrono::steady_clock::time_point::min()) {
    ema_rpm_ = max_rate_ > 0 ? static_cast<double>(max_rate_) : ema_rpm_;
    last_execution_ = now;
    return;
  }
  auto delta = now - last_execution_;
  double minutes = std::chrono::duration<double>(delta).count() / 60.0;
  if (minutes <= std::numeric_limits<double>::epsilon()) {
    minutes = std::numeric_limits<double>::epsilon();
  }
  double rpm = 1.0 / minutes;
  if (ema_rpm_ <= 0.0) {
    ema_rpm_ = rpm;
  } else {
    ema_rpm_ = smoothing_factor_ * rpm + (1.0 - smoothing_factor_) * ema_rpm_;
  }
  last_execution_ = now;
}

std::optional<std::chrono::seconds>
Poller::estimate_clearance_time_unlocked(std::size_t outstanding) const {
  if (outstanding == 0) {
    return std::chrono::seconds(0);
  }
  double rpm = smoothed_requests_per_minute();
  if (rpm <= std::numeric_limits<double>::epsilon()) {
    if (max_rate_ <= 0) {
      return std::nullopt;
    }
    rpm = static_cast<double>(max_rate_);
  }
  double minutes = static_cast<double>(outstanding) / rpm;
  auto seconds = static_cast<long>(std::ceil(std::max(0.0, minutes) * 60.0));
  if (seconds < 0)
    seconds = 0;
  return std::chrono::seconds(seconds);
}

void Poller::check_backlog() {
  auto callback = backlog_callback_;
  if (!callback || backlog_job_threshold_ == 0)
    return;
  auto now = std::chrono::steady_clock::now();
  if (last_backlog_alert_ != std::chrono::steady_clock::time_point::min() &&
      now - last_backlog_alert_ < backlog_alert_cooldown_) {
    return;
  }
  auto outstanding = outstanding_jobs();
  if (outstanding < backlog_job_threshold_)
    return;
  auto clearance = estimate_clearance_time_unlocked(outstanding);
  if (!clearance || *clearance < backlog_time_threshold_)
    return;
  last_backlog_alert_ = now;
  callback(outstanding, *clearance);
}

} // namespace agpm
