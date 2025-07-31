#ifndef AUTOGITHUBPULLMERGE_POLLER_HPP
#define AUTOGITHUBPULLMERGE_POLLER_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

namespace agpm {

/**
 * Simple polling helper running a callback in a worker thread and
 * enforcing a maximum request rate using a token bucket.
 */
class Poller {
public:
  /**
   * Construct a poller.
   *
   * @param task Function to invoke periodically
   * @param interval_ms Poll interval in milliseconds
   * @param max_rate Maximum allowed requests per minute
   */
  Poller(std::function<void()> task, int interval_ms, int max_rate);

  /// Destructor stops the polling thread.
  ~Poller();

  /// Start the polling thread.
  void start();

  /// Stop the polling thread.
  void stop();

private:
  void run();

  std::function<void()> task_;
  int interval_ms_;
  int max_rate_;
  std::atomic<bool> running_{false};
  std::thread thread_;

  double tokens_;
  double capacity_;
  double fill_rate_;
  std::chrono::steady_clock::time_point last_fill_;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_POLLER_HPP
