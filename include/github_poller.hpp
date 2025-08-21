#ifndef AUTOGITHUBPULLMERGE_GITHUB_POLLER_HPP
#define AUTOGITHUBPULLMERGE_GITHUB_POLLER_HPP

#include "github_client.hpp"
#include "history.hpp"
#include "poller.hpp"
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace agpm {

/** Polls GitHub repositories periodically using a token bucket. */
class GitHubPoller {
public:
  /**
   * Construct a poller.
   *
   * @param client GitHub API client
   * @param repos List of {owner, repo} pairs to poll
   * @param interval_ms Poll interval in milliseconds
   * @param max_rate Maximum requests per minute
   */
  GitHubPoller(GitHubClient &client,
               std::vector<std::pair<std::string, std::string>> repos,
               int interval_ms, int max_rate, bool only_poll_prs = false,
               bool only_poll_stray = false, bool reject_dirty = false,
               std::string purge_prefix = "", bool auto_merge = false,
               bool purge_only = false, std::string sort_mode = "",
               PullRequestHistory *history = nullptr);

  /// Start polling in a background thread.
  void start();
  /// Stop polling.
  void stop();

  /// Invoke the polling routine immediately on the current thread.
  void poll_now();

  /// Set a callback invoked with the current pull requests after each poll.
  void
  set_pr_callback(std::function<void(const std::vector<PullRequest> &)> cb);

  /// Set a callback invoked for log messages produced during polling.
  void set_log_callback(std::function<void(const std::string &)> cb);

  /// Set a callback invoked after each poll to export stored history.
  void set_export_callback(std::function<void()> cb);

private:
  void poll();

  GitHubClient &client_;
  std::vector<std::pair<std::string, std::string>> repos_;
  Poller poller_;
  bool only_poll_prs_;
  bool only_poll_stray_;
  bool reject_dirty_;
  std::string purge_prefix_;
  bool auto_merge_;
  bool purge_only_;
  std::string sort_mode_;

  PullRequestHistory *history_;

  std::function<void()> export_cb_;

  std::function<void(const std::vector<PullRequest> &)> pr_cb_;
  std::function<void(const std::string &)> log_cb_;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_GITHUB_POLLER_HPP
