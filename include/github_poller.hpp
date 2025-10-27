#ifndef AUTOGITHUBPULLMERGE_GITHUB_POLLER_HPP
#define AUTOGITHUBPULLMERGE_GITHUB_POLLER_HPP

#include "github_client.hpp"
#include "history.hpp"
#include "hook.hpp"
#include "notification.hpp"
#include "poller.hpp"
#include "rule_engine.hpp"
#include "stray_detection_mode.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace agpm {

struct RepositoryOptions {
  bool only_poll_prs{false};
  bool only_poll_stray{false};
  bool purge_only{false};
  bool auto_merge{false};
  bool reject_dirty{false};
  bool delete_stray{false};
  bool hooks_enabled{true};
  std::string purge_prefix;
};

using RepositoryOptionsMap =
    std::unordered_map<std::string, RepositoryOptions>;

/**
 * Polls GitHub repositories periodically using a token bucket rate limiter.
 */
class GitHubPoller {
public:
  /**
   * Construct a poller.
   *
   * @param client GitHub API client used to perform REST operations.
   * @param repos List of `{owner, repo}` pairs to poll on each iteration.
   * @param interval_ms Base poll interval in milliseconds.
   * @param max_rate Maximum REST requests per minute permitted by the token
   *        bucket.
   * @param hourly_request_limit Maximum REST requests per hour (0 = auto).
   * @param workers Number of worker threads used to fetch repositories in
   *        parallel.
   * @param only_poll_prs When true, skip branch polling entirely.
   * @param only_poll_stray When true, only poll branches for stray detection.
   * @param stray_detection_mode Selects rule-based, heuristic, or combined
   *        stray branch detection.
   * @param reject_dirty Automatically close or delete dirty branches.
   * @param purge_prefix Delete merged branches starting with this prefix.
   * @param auto_merge Automatically merge qualifying pull requests.
   * @param purge_only Only purge branches without polling PRs.
   * @param sort_mode Sort mode for pull request listing.
   * @param history Optional PR history database for export.
   * @param protected_branches Glob patterns for branches that must not be
   *        removed.
   * @param protected_branch_excludes Patterns that override protections.
   * @param dry_run When true, destructive actions are logged but not executed.
   * @param graphql_client Optional GraphQL client used for pull request
   *        listing.
   * @param delete_stray Delete stray branches without requiring a prefix.
   * @param rate_limit_margin Fraction of the detected hourly budget reserved
   *        to avoid exhausting limits.
   * @param rate_limit_refresh_interval Seconds between `/rate_limit` probes.
   * @param retry_rate_limit_endpoint Toggle enabling retries after a
   *        `/rate_limit` failure.
   * @param rate_limit_retry_limit Maximum number of scheduled retries for the
   *        rate limit endpoint when retries are enabled.
   */
  GitHubPoller(
      GitHubClient &client,
      std::vector<std::pair<std::string, std::string>> repos, int interval_ms,
      int max_rate, int hourly_request_limit, int workers = 1,
      bool only_poll_prs = false, bool only_poll_stray = false,
      StrayDetectionMode stray_detection_mode = StrayDetectionMode::RuleBased,
      bool reject_dirty = false, std::string purge_prefix = "",
      bool auto_merge = false, bool purge_only = false,
      std::string sort_mode = "", PullRequestHistory *history = nullptr,
      std::vector<std::string> protected_branches = {},
      std::vector<std::string> protected_branch_excludes = {},
      bool dry_run = false, GitHubGraphQLClient *graphql_client = nullptr,
      bool delete_stray = false, double rate_limit_margin = 0.7,
      std::chrono::seconds rate_limit_refresh_interval =
          std::chrono::seconds(60),
      bool retry_rate_limit_endpoint = false, int rate_limit_retry_limit = 3,
      RepositoryOptionsMap repo_overrides = {});

  /// Start polling in a background thread.
  void start();
  /// Stop polling.
  void stop();

  /// Invoke the polling routine immediately on the current thread.
  void poll_now();

  /**
   * Set a callback invoked with the current pull requests after each poll.
   *
   * @param cb Function receiving the latest pull request list by const
   *        reference.
   */
  void
  set_pr_callback(std::function<void(const std::vector<PullRequest> &)> cb);

  /**
   * Set a callback invoked for log messages produced during polling.
   *
   * @param cb Function receiving log messages.
   */
  void set_log_callback(std::function<void(const std::string &)> cb);

  /**
   * Set a callback invoked after each poll to export stored history.
   *
   * @param cb Function to run after each poll cycle.
   */
  void set_export_callback(std::function<void()> cb);

  /**
   * Set a notifier invoked when merges or branch purges occur.
   *
   * @param notifier Notification handler responsible for user-facing alerts.
   */
  void set_notifier(NotifierPtr notifier);

  /**
   * Set a callback invoked with the latest stray branch list after polling.
   *
   * @param cb Function receiving detected stray branches for display.
   */
  void
  set_stray_callback(std::function<void(const std::vector<StrayBranch> &)> cb);

  /// Override the configured action for a branch state.
  void set_branch_rule_action(const std::string &state, BranchAction action);

  /// Attach a hook dispatcher for external event handling.
  void set_hook_dispatcher(std::shared_ptr<HookDispatcher> dispatcher);

  /// Configure thresholds for aggregate hook events.
  void set_hook_thresholds(int pull_threshold, int branch_threshold);

  /// Retrieve the current scheduler queue snapshot for UI consumption.
  Poller::RequestQueueSnapshot request_queue_snapshot() const {
    return poller_.request_snapshot();
  }

  /// Aggregated view of the latest rate limit budget calculation.
  struct RateBudgetSnapshot {
    long limit{0};
    long remaining{0};
    long used{0};
    long reserve{0};
    long usable{0};
    double minutes_until_reset{0.0};
    double allowed_rpm{0.0};
    double projected_rpm{0.0};
    std::string source;
    bool monitor_enabled{true};
  };

  /// Return the most recently computed rate budget snapshot, if available.
  std::optional<RateBudgetSnapshot> rate_budget_snapshot() const;

private:
  void poll();

  /**
   * Refresh rate limit information and tune scheduler parameters.
   *
   * Queries GitHub when available, computes a conservative request ceiling
   * honouring the configured margin, and adjusts both the worker pool rate and
   * poll interval to avoid exceeding the detected hourly budget.
   */
  void adjust_rate_budget();

  /**
   * Emit a backlog warning describing current scheduler pressure.
   *
   * @param outstanding Number of outstanding jobs queued or executing.
   * @param clearance_estimate Estimated time required to drain the backlog.
   */
  void handle_backlog(std::size_t outstanding,
                      std::chrono::seconds clearance_estimate);

  GitHubClient &client_;
  std::vector<std::pair<std::string, std::string>> repos_;
  Poller poller_;
  int interval_ms_;
  int base_interval_ms_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  int max_rate_;
  int base_max_rate_;
  int hourly_request_limit_;
  int fallback_hourly_limit_;
  bool only_poll_prs_;
  bool only_poll_stray_;
  StrayDetectionMode stray_detection_mode_;
  bool reject_dirty_;
  bool delete_stray_;
  std::string purge_prefix_;
  bool auto_merge_;
  bool purge_only_;
  std::string sort_mode_;
  bool dry_run_;
  GitHubGraphQLClient *graphql_client_;
  PullRequestRuleEngine rule_engine_;
  BranchRuleEngine branch_rule_engine_;
  std::unordered_set<std::string> explicit_branch_rule_states_;

  std::vector<std::string> protected_branches_;
  std::vector<std::string> protected_branch_excludes_;

  PullRequestHistory *history_;

  std::function<void()> export_cb_;

  std::function<void(const std::vector<PullRequest> &)> pr_cb_;
  std::function<void(const std::string &)> log_cb_;
  std::function<void(const std::vector<StrayBranch> &)> stray_cb_;
  NotifierPtr notifier_;
  std::shared_ptr<HookDispatcher> hook_;
  int hook_pull_threshold_{0};
  int hook_branch_threshold_{0};
  bool hook_pull_threshold_triggered_{false};
  bool hook_branch_threshold_triggered_{false};

  std::chrono::steady_clock::duration min_poll_interval_{};
  std::chrono::steady_clock::time_point next_allowed_poll_{};
  std::mutex poll_rate_mutex_;
  double rate_limit_margin_;
  std::chrono::steady_clock::time_point last_budget_refresh_{};
  std::chrono::seconds budget_refresh_period_{std::chrono::seconds(60)};
  bool adaptive_rate_limit_{true};
  bool retry_rate_limit_endpoint_{false};
  int rate_limit_retry_limit_{3};
  int consecutive_rate_limit_failures_{0};
  bool rate_limit_monitor_enabled_{true};
  int rate_limit_query_attempts_{1};
  std::chrono::milliseconds min_request_delay_{0};
  bool fast_mode_{false};

  mutable std::mutex budget_mutex_;
  std::optional<RateBudgetSnapshot> last_budget_snapshot_;

  std::unordered_map<std::string, std::unordered_set<std::string>>
      known_branches_;
  std::mutex known_branches_mutex_;
  RepositoryOptionsMap repo_overrides_;

  RepositoryOptions effective_repository_options(const std::string &owner,
                                                 const std::string &repo) const;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_GITHUB_POLLER_HPP
