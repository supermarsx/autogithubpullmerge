#ifndef AUTOGITHUBPULLMERGE_POLLER_HPP
#define AUTOGITHUBPULLMERGE_POLLER_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

namespace agpm {

/**
 * Thread pool executing submitted polling tasks across multiple workers while
 * enforcing a maximum request rate using a token bucket.
 */
class Poller {
public:
  /**
   * Construct a thread pool and request scheduler.
   *
   * @param workers Number of worker threads used to execute polling jobs.
   * @param max_rate Maximum allowed requests per minute (0 = unlimited).
   * @param smoothing_factor Exponential moving-average factor in the range
   *        (0, 1] applied to request rate sampling.
   */
  Poller(int workers, int max_rate, double smoothing_factor = 0.2);

  /// Destructor stops the worker threads.
  ~Poller();

  /// Start the worker threads.
  void start();

  /// Stop the worker threads.
  void stop();

  /**
   * Submit a task for execution.
   *
   * @param job Callable to execute on one of the worker threads.
   * @return Future that becomes ready once the task completes.
   */
  std::future<void> submit(std::function<void()> job);

  /**
   * Adjust the maximum request rate enforced by the token bucket.
   *
   * @param max_rate Updated requests-per-minute ceiling (0 disables the
   *        limiter).
   */
  void set_max_rate(int max_rate);

  /**
   * Update the exponential smoothing factor used for rate estimation.
   *
   * @param factor Value in the range (0, 1] weighting the most recent
   *        observation.
   */
  void set_smoothing_factor(double factor);

  /**
   * Retrieve the exponentially smoothed requests-per-minute estimate.
   *
   * @return Moving-average request rate reflecting recent executions.
   */
  double smoothed_requests_per_minute() const;

  /**
   * Return the number of queued plus in-flight jobs managed by the scheduler.
   *
   * @return Outstanding job count awaiting execution or completion.
   */
  std::size_t outstanding_jobs() const;

  /**
   * Estimate the amount of time required to drain outstanding jobs.
   *
   * @return Optional clearance estimate rounded to seconds; `std::nullopt`
   *         indicates insufficient data to compute a projection.
   */
  std::optional<std::chrono::seconds> estimate_clearance_time() const;

  /**
   * Configure backlog alert thresholds and notification callback.
   *
   * @param job_threshold Minimum number of outstanding jobs required to
   *        trigger the alert.
   * @param clearance_threshold Minimum estimated clearance time that must be
   *        exceeded before the callback is invoked.
   * @param cb Callback receiving the current backlog size and clearance
   *        estimate when thresholds are met.
   */
  void
  set_backlog_alert(std::size_t job_threshold,
                    std::chrono::seconds clearance_threshold,
                    std::function<void(std::size_t, std::chrono::seconds)> cb);

private:
  void worker();
  bool acquire_token();
  void record_execution();
  void check_backlog();
  std::optional<std::chrono::seconds>
  estimate_clearance_time_unlocked(std::size_t outstanding) const;
  void update_queue_margin();

  int workers_;
  int max_rate_;
  std::atomic<bool> running_{false};
  std::vector<std::thread> threads_;
  std::queue<std::function<void()>> jobs_;
  std::mutex mutex_;
  std::condition_variable cv_;

  // Token bucket
  std::mutex rate_mutex_;
  std::chrono::steady_clock::duration min_interval_{};
  std::chrono::steady_clock::time_point next_allowed_{};
  std::chrono::steady_clock::duration queue_margin_{};
  double queue_balance_slack_{0.1};

  // Scheduler statistics
  double smoothing_factor_;
  mutable std::mutex stats_mutex_;
  std::chrono::steady_clock::time_point last_execution_{};
  double ema_rpm_{0.0};
  std::atomic<std::size_t> queued_{0};
  std::atomic<std::size_t> in_flight_{0};

  // Backlog alerting
  std::size_t backlog_job_threshold_{0};
  std::chrono::seconds backlog_time_threshold_{0};
  std::function<void(std::size_t, std::chrono::seconds)> backlog_callback_;
  std::chrono::steady_clock::time_point last_backlog_alert_{};
  std::chrono::seconds backlog_alert_cooldown_{std::chrono::seconds(30)};
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_POLLER_HPP
