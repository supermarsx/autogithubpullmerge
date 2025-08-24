#ifndef AUTOGITHUBPULLMERGE_POLLER_HPP
#define AUTOGITHUBPULLMERGE_POLLER_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
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
   * Construct a thread pool.
   *
   * @param workers Number of worker threads
   * @param max_rate Maximum allowed requests per minute (0 = unlimited)
   */
  Poller(int workers, int max_rate);

  /// Destructor stops the worker threads.
  ~Poller();

  /// Start the worker threads.
  void start();

  /// Stop the worker threads.
  void stop();

  /// Submit a task for execution.
  std::future<void> submit(std::function<void()> job);

private:
  void worker();
  void acquire_token();

  int workers_;
  int max_rate_;
  std::atomic<bool> running_{false};
  std::vector<std::thread> threads_;
  std::queue<std::function<void()>> jobs_;
  std::mutex mutex_;
  std::condition_variable cv_;

  // Token bucket
  std::mutex rate_mutex_;
  double tokens_;
  double capacity_;
  double fill_rate_;
  std::chrono::steady_clock::time_point last_fill_;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_POLLER_HPP
