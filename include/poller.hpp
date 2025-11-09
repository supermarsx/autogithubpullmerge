/**
 * @file poller.hpp
 * @brief Thread pool and request scheduler for polling tasks with rate limiting.
 *
 * Defines the Poller class, which manages a pool of worker threads to execute polling jobs,
 * enforces a maximum request rate using a token bucket, and provides backlog alerting and
 * statistics for outstanding jobs.
 */
#ifndef AUTOGITHUBPULLMERGE_POLLER_HPP
#define AUTOGITHUBPULLMERGE_POLLER_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace agpm {

/**
 * Thread pool executing submitted polling tasks across multiple workers while
 * enforcing a maximum request rate using a token bucket.
 */
class Poller {
public:
  /// Enumeration describing the lifecycle state of a scheduled request.
  enum class RequestState { Pending, Running, Completed, Failed, Cancelled };

  /** Metadata describing a scheduled request. */
  struct RequestInfo {
    std::size_t id{0};
    std::string name;
    RequestState state{RequestState::Pending};
    std::chrono::steady_clock::time_point enqueued_at{};
    std::optional<std::chrono::steady_clock::time_point> started_at;
    std::optional<std::chrono::steady_clock::time_point> finished_at;
    std::optional<std::chrono::steady_clock::duration> duration;
    std::string error;
  };

  /**
   * Snapshot of the scheduler request queue and aggregate statistics.
   */
  struct RequestQueueSnapshot {
    std::chrono::steady_clock::time_point session_start{};
    std::vector<RequestInfo> pending;
    std::vector<RequestInfo> running;
    std::vector<RequestInfo> completed;
    std::size_t total_completed{0};
    std::size_t total_failed{0};
    std::optional<double> average_latency_ms;
    std::optional<std::chrono::seconds> clearance;
  };

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
  std::future<void> submit(std::string name, std::function<void()> job);

  /// Convenience overload that uses an auto-generated friendly name.
  std::future<void> submit(std::function<void()> job) {
    return submit({}, std::move(job));
  }

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

  /// Capture a snapshot of pending, running, and completed requests.
  RequestQueueSnapshot request_snapshot() const;

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
  std::shared_ptr<RequestInfo> create_request_info(std::string name);
  void mark_started(const std::shared_ptr<RequestInfo> &info,
                    std::chrono::steady_clock::time_point start);
  void mark_finished(const std::shared_ptr<RequestInfo> &info,
                     std::chrono::steady_clock::time_point finish,
                     RequestState state, std::string error);
  void mark_cancelled(const std::shared_ptr<RequestInfo> &info);
  void trim_completed_history();

  int workers_;
  int max_rate_;
  std::atomic<bool> running_{false};
  std::vector<std::thread> threads_;
  struct ScheduledJob {
    std::shared_ptr<RequestInfo> info;
    std::shared_ptr<std::packaged_task<void()>> task;
  };
  std::queue<ScheduledJob> jobs_;
  std::deque<std::shared_ptr<RequestInfo>> pending_infos_;
  std::vector<std::shared_ptr<RequestInfo>> active_infos_;
  std::deque<std::shared_ptr<RequestInfo>> completed_infos_;
  mutable std::mutex mutex_;
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
  std::atomic<std::size_t> next_request_id_{1};
  std::chrono::steady_clock::time_point session_start_{};
  std::chrono::steady_clock::duration total_latency_{};
  std::size_t latency_samples_{0};
  std::size_t total_completed_{0};
  std::size_t total_failed_{0};
  std::size_t completed_history_limit_{64};

  // Backlog alerting
  std::size_t backlog_job_threshold_{0};
  std::chrono::seconds backlog_time_threshold_{0};
  std::function<void(std::size_t, std::chrono::seconds)> backlog_callback_;
  std::chrono::steady_clock::time_point last_backlog_alert_{};
  std::chrono::seconds backlog_alert_cooldown_{std::chrono::seconds(30)};
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_POLLER_HPP
