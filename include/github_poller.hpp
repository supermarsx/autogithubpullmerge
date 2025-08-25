#ifndef AUTOGITHUBPULLMERGE_GITHUB_POLLER_HPP
#define AUTOGITHUBPULLMERGE_GITHUB_POLLER_HPP

#include "github_client.hpp"
#include "history.hpp"
#include "poller.hpp"
#include <atomic>
#include <functional>
#include <string>
#include <thread>
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
   * @param only_poll_prs When true, skip branch polling
   * @param only_poll_stray When true, only poll branches for stray detection
   * @param reject_dirty Automatically close or delete dirty branches
   * @param purge_prefix Delete merged branches starting with this prefix
   * @param auto_merge Automatically merge qualifying pull requests
   * @param purge_only Only purge branches without polling PRs
   * @param sort_mode Sort mode for pull request listing
   * @param history Optional PR history database for export
   * @param protected_branches Glob patterns for branches that must not be
   *        removed
   * @param protected_branch_excludes Patterns that override protections
   */
  GitHubPoller(GitHubClient &client,
               std::vector<std::pair<std::string, std::string>> repos,
               int interval_ms, int max_rate, int workers = 1,
               bool only_poll_prs = false, bool only_poll_stray = false,
               bool reject_dirty = false, std::string purge_prefix = "",
               bool auto_merge = false, bool purge_only = false,
               std::string sort_mode = "",
               PullRequestHistory *history = nullptr,
               std::vector<std::string> protected_branches = {},
               std::vector<std::string> protected_branch_excludes = {},
               bool dry_run = false);

  /// Start polling in a background thread.
  void start();
  /// Stop polling.
  void stop();

  /// Invoke the polling routine immediately on the current thread.
  void poll_now();

  /**
   * Set a callback invoked with the current pull requests after each poll.
   *
   * @param cb Function receiving the latest pull request list
   */
  void
  set_pr_callback(std::function<void(const std::vector<PullRequest> &)> cb);

  /**
   * Set a callback invoked for log messages produced during polling.
   *
   * @param cb Function receiving log messages
   */
  void set_log_callback(std::function<void(const std::string &)> cb);

  /**
   * Set a callback invoked after each poll to export stored history.
   *
   * @param cb Function to run after each poll cycle
   */
  void set_export_callback(std::function<void()> cb);

private:
  void poll();

  GitHubClient &client_;
  std::vector<std::pair<std::string, std::string>> repos_;
  Poller poller_;
  int interval_ms_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  bool only_poll_prs_;
  bool only_poll_stray_;
  bool reject_dirty_;
  std::string purge_prefix_;
  bool auto_merge_;
  bool purge_only_;
  std::string sort_mode_;
  bool dry_run_;

  std::vector<std::string> protected_branches_;
  std::vector<std::string> protected_branch_excludes_;

  PullRequestHistory *history_;

  std::function<void()> export_cb_;

  std::function<void(const std::vector<PullRequest> &)> pr_cb_;
  std::function<void(const std::string &)> log_cb_;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_GITHUB_POLLER_HPP
