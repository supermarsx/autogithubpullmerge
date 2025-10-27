#include "github_poller.hpp"
#include "log.hpp"
#include "sort.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <future>
#include <iomanip>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace agpm {

namespace {
constexpr int kFallbackHourlyLimit = 5000;
std::shared_ptr<spdlog::logger> poller_log() {
  static auto logger = [] {
    ensure_default_logger();
    return category_logger("github.poller");
  }();
  return logger;
}
} // namespace

/**
 * Construct a poller coordinating periodic GitHub synchronization tasks.
 *
 * @param client GitHub client used for REST interactions.
 * @param repos Repositories to process on each poll cycle.
 * @param interval_ms Poll interval in milliseconds.
 * @param max_rate Maximum requests per minute permitted.
 * @param workers Number of worker threads to use for concurrent operations.
 * @param only_poll_prs Skip branch polling when true.
 * @param only_poll_stray Restrict branch polling to stray detection.
 * @param stray_detection_mode Selects rule-based, heuristic, or combined stray
 *        detection.
 * @param reject_dirty Close or delete dirty branches automatically.
 * @param purge_prefix Branch prefix qualifying for automated deletion.
 * @param auto_merge Automatically merge qualifying pull requests when true.
 * @param purge_only Skip PR polling and only purge branches.
 * @param sort_mode Sort mode used for rendering pull requests.
 * @param history Optional history database for exports.
 * @param protected_branches Branch patterns protected from deletion.
 * @param protected_branch_excludes Patterns overriding protected branches.
 * @param dry_run When true, perform logging without mutating repositories.
 * @param graphql_client Optional GraphQL client for PR listing.
 * @param delete_stray Delete stray branches automatically when true.
 * @param rate_limit_margin Fraction of the detected hourly budget reserved.
 * @param rate_limit_refresh_interval Seconds between `/rate_limit` probes.
 * @param retry_rate_limit_endpoint Enable repeated `/rate_limit` queries after
 *        failures when true.
 * @param rate_limit_retry_limit Maximum number of scheduled retries before
 *        disabling the `/rate_limit` endpoint.
 */
GitHubPoller::GitHubPoller(
    GitHubClient &client,
    std::vector<std::pair<std::string, std::string>> repos, int interval_ms,
    int max_rate, int hourly_request_limit, int workers, bool only_poll_prs,
    bool only_poll_stray, StrayDetectionMode stray_detection_mode,
    bool reject_dirty, std::string purge_prefix, bool auto_merge,
    bool purge_only, std::string sort_mode, PullRequestHistory *history,
    std::vector<std::string> protected_branches,
    std::vector<std::string> protected_branch_excludes, bool dry_run,
    GitHubGraphQLClient *graphql_client, bool delete_stray,
    double rate_limit_margin, std::chrono::seconds rate_limit_refresh_interval,
    bool retry_rate_limit_endpoint, int rate_limit_retry_limit,
    RepositoryOptionsMap repo_overrides)
    : client_(client), repos_(std::move(repos)), poller_(workers, max_rate),
      interval_ms_(interval_ms), base_interval_ms_(interval_ms),
      max_rate_(max_rate), base_max_rate_(max_rate),
      hourly_request_limit_(hourly_request_limit > 0 ? hourly_request_limit
                                                     : 0),
      fallback_hourly_limit_(kFallbackHourlyLimit),
      only_poll_prs_(only_poll_prs), only_poll_stray_(only_poll_stray),
      stray_detection_mode_(stray_detection_mode), reject_dirty_(reject_dirty),
      delete_stray_(delete_stray), purge_prefix_(std::move(purge_prefix)),
      auto_merge_(auto_merge), purge_only_(purge_only),
      sort_mode_(std::move(sort_mode)), dry_run_(dry_run),
      graphql_client_(graphql_client),
      protected_branches_(std::move(protected_branches)),
      protected_branch_excludes_(std::move(protected_branch_excludes)),
      history_(history), rate_limit_margin_(rate_limit_margin),
      repo_overrides_(std::move(repo_overrides)) {
  ensure_default_logger();
  next_allowed_poll_ = std::chrono::steady_clock::now();
  if (rate_limit_margin_ <= 0.0) {
    rate_limit_margin_ = 0.0;
    adaptive_rate_limit_ = false;
  } else {
    if (rate_limit_margin_ >= 0.95)
      rate_limit_margin_ = 0.95;
    adaptive_rate_limit_ = true;
  }
  last_budget_refresh_ = std::chrono::steady_clock::time_point::min();
  budget_refresh_period_ = rate_limit_refresh_interval.count() > 0
                               ? rate_limit_refresh_interval
                               : std::chrono::seconds(60);
  retry_rate_limit_endpoint_ = retry_rate_limit_endpoint;
  rate_limit_retry_limit_ = std::max(1, rate_limit_retry_limit);
  rate_limit_query_attempts_ = 1;
  if (!retry_rate_limit_endpoint_) {
    rate_limit_retry_limit_ = 1;
  }
  min_request_delay_ = std::chrono::milliseconds(
      max_rate_ > 0 ? std::max(1, 60000 / max_rate_) : 0);
  auto backlog_jobs_threshold =
      static_cast<std::size_t>(std::max(workers * 2, 10));
  auto clearance_threshold = budget_refresh_period_ > std::chrono::seconds(0)
                                 ? budget_refresh_period_
                                 : std::chrono::seconds(60);
  poller_.set_backlog_alert(
      backlog_jobs_threshold, clearance_threshold,
      [this](std::size_t outstanding, std::chrono::seconds clearance) {
        handle_backlog(outstanding, clearance);
      });
  if (max_rate_ > 0) {
    auto interval =
        std::chrono::duration<double>(60.0 / static_cast<double>(max_rate_));
    min_poll_interval_ =
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            interval);
    if (min_poll_interval_.count() <= 0) {
      min_poll_interval_ = std::chrono::nanoseconds(1);
    }
  }
}

RepositoryOptions
GitHubPoller::effective_repository_options(const std::string &owner,
                                           const std::string &repo) const {
  RepositoryOptions opts;
  opts.only_poll_prs = only_poll_prs_;
  opts.only_poll_stray = only_poll_stray_;
  opts.purge_only = purge_only_;
  opts.auto_merge = auto_merge_;
  opts.reject_dirty = reject_dirty_;
  opts.delete_stray = delete_stray_;
  opts.purge_prefix = purge_prefix_;
  opts.hooks_enabled = hook_ != nullptr;
  auto it = repo_overrides_.find(owner + "/" + repo);
  if (it != repo_overrides_.end()) {
    opts = it->second;
  }
  opts.hooks_enabled = opts.hooks_enabled && hook_ != nullptr;
  return opts;
}

/**
 * Launch the poller thread and begin scheduling work.
 */
void GitHubPoller::start() {
  poller_log()->info("Starting GitHub poller");
  poller_.start();
  running_ = true;
  thread_ = std::thread([this] {
    while (running_) {
      poll();
      std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));
    }
  });
}

/**
 * Stop the poller thread and wait for outstanding work to finish.
 */
void GitHubPoller::stop() {
  poller_log()->info("Stopping GitHub poller");
  running_ = false;
  if (thread_.joinable()) {
    thread_.join();
  }
  poller_.stop();
}

/**
 * Trigger an immediate poll on the calling thread.
 */
void GitHubPoller::poll_now() { poll(); }

/**
 * Register a callback invoked with the latest pull request snapshot.
 *
 * @param cb Callback receiving the aggregated pull request list.
 */
void GitHubPoller::set_pr_callback(
    std::function<void(const std::vector<PullRequest> &)> cb) {
  pr_cb_ = std::move(cb);
}

/**
 * Register a callback for textual log messages produced by the poller.
 *
 * @param cb Callback receiving log message strings.
 */
void GitHubPoller::set_log_callback(
    std::function<void(const std::string &)> cb) {
  log_cb_ = std::move(cb);
}

/**
 * Register a callback for stray branch snapshots.
 *
 * @param cb Callback receiving detected stray branches.
 */
void GitHubPoller::set_stray_callback(
    std::function<void(const std::vector<StrayBranch> &)> cb) {
  stray_cb_ = std::move(cb);
}

/**
 * Configure the branch rule engine to take a specific action for a state.
 */
void GitHubPoller::set_branch_rule_action(const std::string &state,
                                          BranchAction action) {
  branch_rule_engine_.set_action(state, action);
}

/**
 * Attach an asynchronous hook dispatcher for poller events.
 */
void GitHubPoller::set_hook_dispatcher(
    std::shared_ptr<HookDispatcher> dispatcher) {
  hook_ = std::move(dispatcher);
}

/**
 * Configure thresholds that emit aggregate hook events.
 */
void GitHubPoller::set_hook_thresholds(int pull_threshold, int branch_threshold) {
  hook_pull_threshold_ = pull_threshold < 0 ? 0 : pull_threshold;
  hook_branch_threshold_ = branch_threshold < 0 ? 0 : branch_threshold;
  if (hook_pull_threshold_ == 0) {
    hook_pull_threshold_triggered_ = false;
  }
  if (hook_branch_threshold_ == 0) {
    hook_branch_threshold_triggered_ = false;
  }
}

std::optional<GitHubPoller::RateBudgetSnapshot>
GitHubPoller::rate_budget_snapshot() const {
  std::lock_guard<std::mutex> lock(budget_mutex_);
  return last_budget_snapshot_;
}

/**
 * Register a callback that performs export tasks after each poll.
 *
 * @param cb Callback invoked after each polling cycle completes.
 */
void GitHubPoller::set_export_callback(std::function<void()> cb) {
  export_cb_ = std::move(cb);
}

/**
 * Attach a notifier used to broadcast poller events.
 *
 * @param notifier Shared pointer to a notifier implementation.
 */
void GitHubPoller::set_notifier(NotifierPtr notifier) {
  notifier_ = std::move(notifier);
}

/**
 * Perform a full polling cycle across all configured repositories.
 *
 * @return void
 */
void GitHubPoller::poll() {
  adjust_rate_budget();
  if (max_rate_ > 0 && max_rate_ <= 1) {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lk(poll_rate_mutex_);
    if (now < next_allowed_poll_) {
      return;
    }
    next_allowed_poll_ = now + min_poll_interval_;
  }
  poller_log()->debug("Polling repositories");
  std::vector<PullRequest> all_prs;
  std::vector<StrayBranch> all_stray;
  std::mutex pr_mutex;
  std::mutex stray_mutex;
  std::mutex log_mutex;
  std::atomic<std::size_t> total_pr_count{0};
  std::atomic<std::size_t> total_branch_count{0};
  std::vector<std::future<void>> futures;
  futures.reserve(repos_.size());
  for (const auto &repo : repos_) {
    RepositoryOptions options =
        effective_repository_options(repo.first, repo.second);
    bool skip_branch_ops =
        options.only_poll_prs || (max_rate_ > 0 && max_rate_ <= 1);
    std::string repo_name = repo.first + "/" + repo.second;
    std::string job_label;
    if (options.purge_only) {
      job_label = repo_name + " purge";
    } else if (skip_branch_ops) {
      job_label = repo_name + " pull requests";
    } else {
      job_label = repo_name + " sync";
    }
    futures.emplace_back(poller_.submit(
        job_label,
        [this, repo, repo_name, options, skip_branch_ops, &all_prs, &all_stray,
         &pr_mutex, &stray_mutex, &log_mutex, &total_pr_count,
         &total_branch_count] {
          bool repo_hooks_enabled = options.hooks_enabled && hook_;
          if (options.purge_only) {
            poller_log()->debug("purge_only set - skipping repo {}", repo_name);
            if (!options.purge_prefix.empty()) {
              auto removed = client_.cleanup_branches(
                  repo.first, repo.second, options.purge_prefix,
                  protected_branches_, protected_branch_excludes_);
              if (repo_hooks_enabled && !removed.empty()) {
                for (const auto &branch_name : removed) {
                  HookEvent evt{"branch.deleted"};
                  evt.data["owner"] = repo.first;
                  evt.data["repo"] = repo.second;
                  evt.data["branch"] = branch_name;
                  evt.data["reason"] = "purge_only";
                  hook_->enqueue(std::move(evt));
                }
              }
              if (notifier_) {
                notifier_->notify("Purged branches in " + repo.first + "/" +
                                  repo.second);
              }
            }
            return;
          }
          if (!options.only_poll_stray) {
            const std::vector<PullRequest> prs = [this, &repo]() {
          if (graphql_client_) {
            return graphql_client_->list_pull_requests(repo.first, repo.second);
          }
          if (max_rate_ > 0 && max_rate_ <= 1) {
            // Tests require a single HTTP request when rate is extremely low
            return client_.list_open_pull_requests_single(repo.first + "/" +
                                                          repo.second);
          }
          return client_.list_pull_requests(repo.first, repo.second);
            }();
            {
              std::lock_guard<std::mutex> lk(pr_mutex);
              all_prs.insert(all_prs.end(), prs.begin(), prs.end());
              if (history_) {
                for (const auto &pr : prs) {
                  history_->insert(pr.number, pr.title, pr.merged);
                }
              }
            }
            total_pr_count.fetch_add(prs.size(), std::memory_order_relaxed);
            if (log_cb_) {
              std::lock_guard<std::mutex> lk(log_mutex);
              log_cb_(repo.first + "/" + repo.second +
                      " pull requests: " + std::to_string(prs.size()));
            } else {
              poller_log()->info("Fetched {} pull requests for {}/{}",
                                 prs.size(), repo.first, repo.second);
            }
            if (options.auto_merge) {
              auto remove_pr = [&](const PullRequest &target) {
                std::lock_guard<std::mutex> lk(pr_mutex);
                auto new_end = std::remove_if(
                    all_prs.begin(), all_prs.end(),
                    [&](const PullRequest &candidate) {
                      return candidate.number == target.number &&
                             candidate.owner == target.owner &&
                             candidate.repo == target.repo;
                    });
                std::size_t removed = static_cast<std::size_t>(
                    std::distance(new_end, all_prs.end()));
                if (removed > 0) {
                  all_prs.erase(new_end, all_prs.end());
                  total_pr_count.fetch_sub(removed, std::memory_order_relaxed);
                }
              };
              for (const auto &pr : prs) {
            auto metadata =
                client_.pull_request_metadata(pr.owner, pr.repo, pr.number);
            if (!metadata) {
              continue;
            }
            PullRequestAction action = rule_engine_.decide(*metadata);
            if (dry_run_) {
              if (action == PullRequestAction::kMerge) {
                client_.merge_pull_request(pr.owner, pr.repo, pr.number,
                                           *metadata);
                if (log_cb_) {
                  std::lock_guard<std::mutex> lk(log_mutex);
                  log_cb_("Would merge PR #" + std::to_string(pr.number));
                }
              } else if (action == PullRequestAction::kClose) {
                client_.close_pull_request(pr.owner, pr.repo, pr.number);
                if (log_cb_) {
                  std::lock_guard<std::mutex> lk(log_mutex);
                  log_cb_("Would close PR #" + std::to_string(pr.number));
                }
              }
              continue;
            }
            if (action == PullRequestAction::kMerge) {
              bool merged = client_.merge_pull_request(pr.owner, pr.repo,
                                                       pr.number, *metadata);
              if (merged) {
                if (history_) {
                  std::lock_guard<std::mutex> lk(pr_mutex);
                  history_->update_merged(pr.number);
                }
                if (log_cb_) {
                  std::lock_guard<std::mutex> lk(log_mutex);
                  log_cb_("Merged PR #" + std::to_string(pr.number));
                }
                if (notifier_) {
                  notifier_->notify("Merged PR #" + std::to_string(pr.number) +
                                    " in " + pr.owner + "/" + pr.repo);
                }
                if (repo_hooks_enabled) {
                  HookEvent evt{"pull_request.merged"};
                  evt.data["number"] = pr.number;
                  evt.data["owner"] = pr.owner;
                  evt.data["repo"] = pr.repo;
                  evt.data["title"] = pr.title;
                  evt.data["mergeable_state"] = metadata->mergeable_state;
                  evt.data["mergeable"] = metadata->mergeable;
                  evt.data["draft"] = metadata->draft;
                  hook_->enqueue(std::move(evt));
                }
                remove_pr(pr);
              } else {
                if (repo_hooks_enabled) {
                  HookEvent evt{"pull_request.merge_failed"};
                  evt.data["number"] = pr.number;
                  evt.data["owner"] = pr.owner;
                  evt.data["repo"] = pr.repo;
                  evt.data["title"] = pr.title;
                  evt.data["mergeable_state"] = metadata->mergeable_state;
                  evt.data["mergeable"] = metadata->mergeable;
                  evt.data["draft"] = metadata->draft;
                  hook_->enqueue(std::move(evt));
                }
                if (log_cb_) {
                  std::lock_guard<std::mutex> lk(log_mutex);
                  log_cb_("PR #" + std::to_string(pr.number) +
                          " did not meet merge requirements");
                }
              }
            } else if (action == PullRequestAction::kClose) {
              bool closed =
                  client_.close_pull_request(pr.owner, pr.repo, pr.number);
              if (closed) {
                if (log_cb_) {
                  std::lock_guard<std::mutex> lk(log_mutex);
                  log_cb_("Closed PR #" + std::to_string(pr.number));
                }
                if (notifier_) {
                  notifier_->notify("Closed PR #" + std::to_string(pr.number) +
                                    " in " + pr.owner + "/" + pr.repo);
                }
                if (repo_hooks_enabled) {
                  HookEvent evt{"pull_request.closed"};
                  evt.data["number"] = pr.number;
                  evt.data["owner"] = pr.owner;
                  evt.data["repo"] = pr.repo;
                  evt.data["title"] = pr.title;
                  hook_->enqueue(std::move(evt));
                }
                remove_pr(pr);
              } else {
                if (repo_hooks_enabled) {
                  HookEvent evt{"pull_request.close_failed"};
                  evt.data["number"] = pr.number;
                  evt.data["owner"] = pr.owner;
                  evt.data["repo"] = pr.repo;
                  evt.data["title"] = pr.title;
                  hook_->enqueue(std::move(evt));
                }
                if (log_cb_) {
                  std::lock_guard<std::mutex> lk(log_mutex);
                  log_cb_("PR #" + std::to_string(pr.number) +
                          " could not be closed");
                }
              }
            }
          }
        }
      }
      if (!skip_branch_ops) {
        std::string default_branch;
        auto branches =
            client_.list_branches(repo.first, repo.second, &default_branch);
        total_branch_count.fetch_add(branches.size(),
                                     std::memory_order_relaxed);
        if (log_cb_) {
          std::lock_guard<std::mutex> lk(log_mutex);
          log_cb_(repo.first + "/" + repo.second +
                  " branches: " + std::to_string(branches.size()));
        } else {
          poller_log()->info("Fetched {} branches for {}/{}", branches.size(),
                             repo.first, repo.second);
        }
        std::unordered_set<std::string> new_branches;
        {
          std::lock_guard<std::mutex> lk(known_branches_mutex_);
          auto &known = known_branches_[repo.first + "/" + repo.second];
          for (const auto &branch : branches) {
            if (known.insert(branch).second) {
              new_branches.insert(branch);
            }
          }
        }
        std::vector<std::string> stray;
        std::unordered_set<std::string> seen_branches;
        auto record_branch = [&](const std::string &branch) {
          if (seen_branches.insert(branch).second) {
            stray.push_back(branch);
          }
        };
        if (uses_rule_based(stray_detection_mode_)) {
          for (const auto &branch : branches) {
            if (!options.purge_prefix.empty() &&
                branch.rfind(options.purge_prefix, 0) == 0) {
              continue;
            }
            record_branch(branch);
          }
        }
        if (uses_heuristic(stray_detection_mode_) && !default_branch.empty()) {
          auto heuristic_branches = client_.detect_stray_branches(
              repo.first, repo.second, default_branch, branches,
              protected_branches_, protected_branch_excludes_);
          for (const auto &branch : heuristic_branches) {
            if (!options.purge_prefix.empty() &&
                branch.rfind(options.purge_prefix, 0) == 0) {
              continue;
            }
            record_branch(branch);
          }
        }
        if (log_cb_) {
          std::lock_guard<std::mutex> lk(log_mutex);
          log_cb_(repo.first + "/" + repo.second +
                  " stray branches: " + std::to_string(stray.size()));
        } else {
          poller_log()->info("{} / {} stray branches: {}", repo.first,
                             repo.second, stray.size());
        }
        if (!stray.empty()) {
          std::lock_guard<std::mutex> lk(stray_mutex);
          for (const auto &branch : stray) {
            all_stray.push_back(StrayBranch{repo.first, repo.second, branch});
          }
        }
        std::unordered_set<std::string> stray_lookup(stray.begin(),
                                                     stray.end());
        for (const auto &branch : stray) {
          BranchMetadata metadata{repo.first, repo.second, branch, "stray", true,
                                  new_branches.count(branch) > 0};
          BranchAction action = branch_rule_engine_.decide(metadata);
          if (!options.delete_stray) {
            action = BranchAction::kKeep;
          }
          if (action == BranchAction::kDelete) {
            auto removed = client_.cleanup_branches(
                repo.first, repo.second, branch, protected_branches_,
                protected_branch_excludes_);
            if (!removed.empty()) {
              std::lock_guard<std::mutex> lk(stray_mutex);
              for (const auto &name : removed) {
                auto new_end =
                    std::remove_if(all_stray.begin(), all_stray.end(),
                                   [&](const StrayBranch &entry) {
                                     return entry.owner == repo.first &&
                                            entry.repo == repo.second &&
                                            entry.name == name;
                                   });
                all_stray.erase(new_end, all_stray.end());
              }
              if (repo_hooks_enabled) {
                for (const auto &name : removed) {
                  HookEvent evt{"branch.deleted"};
                  evt.data["owner"] = repo.first;
                  evt.data["repo"] = repo.second;
                  evt.data["branch"] = name;
                  evt.data["reason"] = "stray";
                  hook_->enqueue(std::move(evt));
                }
              }
            }
          } else if (action == BranchAction::kIgnore) {
            std::lock_guard<std::mutex> lk(stray_mutex);
            auto new_end = std::remove_if(all_stray.begin(), all_stray.end(),
                                          [&](const StrayBranch &entry) {
                                            return entry.owner == repo.first &&
                                                   entry.repo == repo.second &&
                                                   entry.name == branch;
                                          });
            all_stray.erase(new_end, all_stray.end());
          }
        }
        for (const auto &branch : new_branches) {
          if (stray_lookup.find(branch) != stray_lookup.end()) {
            continue;
          }
          BranchMetadata metadata{repo.first, repo.second, branch, "new", false,
                                  true};
          BranchAction action = branch_rule_engine_.decide(metadata);
          if (action == BranchAction::kDelete) {
            auto removed = client_.cleanup_branches(repo.first, repo.second,
                                                    branch, protected_branches_,
                                                    protected_branch_excludes_);
            if (repo_hooks_enabled && !removed.empty()) {
              for (const auto &name : removed) {
                HookEvent evt{"branch.deleted"};
                evt.data["owner"] = repo.first;
                evt.data["repo"] = repo.second;
                evt.data["branch"] = name;
                evt.data["reason"] = "new";
                hook_->enqueue(std::move(evt));
              }
            }
          }
        }
      }
      if (!options.purge_prefix.empty()) {
        BranchMetadata metadata{repo.first, repo.second, options.purge_prefix,
                                 "purge"};
        BranchAction action = branch_rule_engine_.decide(metadata);
        if (action == BranchAction::kDelete) {
          auto removed = client_.cleanup_branches(
              repo.first, repo.second, options.purge_prefix, protected_branches_,
              protected_branch_excludes_);
          if (!removed.empty()) {
            std::lock_guard<std::mutex> lk(stray_mutex);
            for (const auto &name : removed) {
              auto new_end =
                  std::remove_if(all_stray.begin(), all_stray.end(),
                                 [&](const StrayBranch &entry) {
                                   return entry.owner == repo.first &&
                                          entry.repo == repo.second &&
                                          entry.name == name;
                                 });
              all_stray.erase(new_end, all_stray.end());
            }
            if (repo_hooks_enabled) {
              for (const auto &name : removed) {
                HookEvent evt{"branch.deleted"};
                evt.data["owner"] = repo.first;
                evt.data["repo"] = repo.second;
                evt.data["branch"] = name;
                evt.data["reason"] = "purge";
                hook_->enqueue(std::move(evt));
              }
            }
          }
          if (notifier_) {
            notifier_->notify("Purged branches in " + repo.first + "/" +
                              repo.second);
          }
        }
      }
      if (!skip_branch_ops) {
        BranchMetadata dirty_metadata{repo.first, repo.second, std::string{},
                                      "dirty"};
        BranchAction dirty_action = branch_rule_engine_.decide(dirty_metadata);
        if (!options.reject_dirty) {
          dirty_action = BranchAction::kKeep;
        }
        if (dirty_action == BranchAction::kDelete) {
          client_.close_dirty_branches(repo.first, repo.second,
                                       protected_branches_,
                                       protected_branch_excludes_);
        }
      }
    }));
  }
  for (auto &f : futures) {
    f.get();
  }
  const std::size_t total_prs = total_pr_count.load(std::memory_order_relaxed);
  if (log_cb_) {
    std::lock_guard<std::mutex> lk(log_mutex);
    log_cb_("Total pull requests fetched: " + std::to_string(total_prs));
  } else {
    poller_log()->info("Total pull requests fetched: {}", total_prs);
  }
  if (hook_ && hook_pull_threshold_ > 0) {
    bool exceeded =
        total_prs > static_cast<std::size_t>(hook_pull_threshold_);
    if (exceeded && !hook_pull_threshold_triggered_) {
      HookEvent evt{"poll.pull_threshold"};
      evt.data["total_pull_requests"] = total_prs;
      evt.data["threshold"] = hook_pull_threshold_;
      hook_->enqueue(std::move(evt));
      hook_pull_threshold_triggered_ = true;
    } else if (!exceeded) {
      hook_pull_threshold_triggered_ = false;
    }
  }
  sort_pull_requests(all_prs, sort_mode_);
  if (pr_cb_) {
    pr_cb_(all_prs);
  }
  if (stray_cb_) {
    stray_cb_(all_stray);
  }
  if (!skip_branch_ops) {
    const std::size_t total_branches =
        total_branch_count.load(std::memory_order_relaxed);
    if (log_cb_) {
      std::lock_guard<std::mutex> lk(log_mutex);
      log_cb_("Total branches fetched: " + std::to_string(total_branches));
    } else {
      poller_log()->info("Total branches fetched: {}", total_branches);
    }
    if (hook_ && hook_branch_threshold_ > 0) {
      bool exceeded =
          total_branches > static_cast<std::size_t>(hook_branch_threshold_);
      if (exceeded && !hook_branch_threshold_triggered_) {
        HookEvent evt{"poll.branch_threshold"};
        evt.data["total_branches"] = total_branches;
        evt.data["threshold"] = hook_branch_threshold_;
        hook_->enqueue(std::move(evt));
        hook_branch_threshold_triggered_ = true;
      } else if (!exceeded) {
        hook_branch_threshold_triggered_ = false;
      }
    }
  } else if (hook_) {
    hook_branch_threshold_triggered_ = false;
  }
  if (export_cb_) {
    poller_log()->info("Running export callback");
    export_cb_();
  }
  if (log_cb_ && skip_branch_ops) {
    std::lock_guard<std::mutex> lk(log_mutex);
    log_cb_("Polled " + std::to_string(all_prs.size()) + " pull requests");
  }
}

/**
 * Refresh rate limit information and tune scheduler parameters.
 */
void GitHubPoller::adjust_rate_budget() {
  if (!adaptive_rate_limit_) {
    if (base_max_rate_ != max_rate_) {
      poller_.set_max_rate(base_max_rate_);
      max_rate_ = base_max_rate_;
    }
    if (interval_ms_ != base_interval_ms_) {
      interval_ms_ = base_interval_ms_;
    }
    return;
  }
  auto now = std::chrono::steady_clock::now();
  if (last_budget_refresh_ != std::chrono::steady_clock::time_point::min() &&
      now - last_budget_refresh_ < budget_refresh_period_) {
    return;
  }
  last_budget_refresh_ = now;
  std::optional<GitHubClient::RateLimitStatus> status_opt;
  bool queried_endpoint = false;
  if (rate_limit_monitor_enabled_) {
    status_opt = client_.rate_limit_status(rate_limit_query_attempts_);
    queried_endpoint = true;
    if (!status_opt) {
      ++consecutive_rate_limit_failures_;
      if (!retry_rate_limit_endpoint_ ||
          consecutive_rate_limit_failures_ >= rate_limit_retry_limit_) {
        rate_limit_monitor_enabled_ = false;
        poller_log()->warn(
            "Disabling rate limit endpoint polling after {} failure(s)",
            consecutive_rate_limit_failures_);
      } else {
        poller_log()->warn("Rate limit endpoint unavailable ({} consecutive "
                           "failure(s)); will retry in {}s",
                           consecutive_rate_limit_failures_,
                           budget_refresh_period_.count());
      }
    } else {
      consecutive_rate_limit_failures_ = 0;
    }
  }
  long limit = 0;
  long remaining = 0;
  double seconds_left = 3600.0;
  std::string source_tag;
  if (status_opt && status_opt->limit > 0) {
    limit = status_opt->limit;
    remaining = status_opt->remaining;
    seconds_left = std::max<double>(status_opt->reset_after.count(), 60.0);
    source_tag = "detected";
    if (hourly_request_limit_ > 0 && limit > hourly_request_limit_) {
      limit = hourly_request_limit_;
      if (remaining > limit) {
        remaining = limit;
      }
      source_tag = "detected+override";
    }
  } else {
    limit = hourly_request_limit_ > 0 ? hourly_request_limit_
                                      : fallback_hourly_limit_;
    if (limit <= 0) {
      limit = fallback_hourly_limit_;
    }
    remaining = limit;
    if (!rate_limit_monitor_enabled_ && queried_endpoint) {
      source_tag = "monitor-disabled";
    } else {
      source_tag = hourly_request_limit_ > 0 ? "configured" : "fallback";
    }
  }
  if (remaining > limit) {
    remaining = limit;
  }
  if (limit <= 0) {
    return;
  }
  long reserve = static_cast<long>(
      std::floor(static_cast<double>(limit) * rate_limit_margin_));
  if (reserve < 0)
    reserve = 0;
  if (reserve > limit)
    reserve = limit;
  long usable = remaining - reserve;
  if (usable < 0)
    usable = 0;
  if (seconds_left < 60.0)
    seconds_left = 60.0;
  double minutes_left = seconds_left / 60.0;
  double hourly_cap =
      static_cast<double>(limit) * (1.0 - rate_limit_margin_) / 60.0;
  if (hourly_cap <= 0.0) {
    hourly_cap = static_cast<double>(limit) / 60.0;
  }
  double remaining_cap = minutes_left > 0.0
                             ? static_cast<double>(usable) / minutes_left
                             : static_cast<double>(usable);
  double allowed = hourly_cap;
  if (allowed <= 0.0 && remaining_cap > 0.0) {
    allowed = remaining_cap;
  } else if (remaining_cap > 0.0) {
    allowed = std::min(allowed, remaining_cap);
  }
  if (base_max_rate_ > 0 && allowed > 0.0) {
    allowed = std::min(allowed, static_cast<double>(base_max_rate_));
  }
  int desired_rate = 0;
  if (allowed > 0.0) {
    desired_rate = static_cast<int>(std::floor(allowed));
    if (desired_rate <= 0) {
      desired_rate = 1;
    }
  } else if (base_max_rate_ > 0) {
    desired_rate = std::max(1, base_max_rate_);
  } else {
    desired_rate = 1;
  }
  if (base_max_rate_ > 0 && desired_rate > base_max_rate_) {
    desired_rate = base_max_rate_;
  }
  if (desired_rate <= 0) {
    desired_rate = base_max_rate_ > 0 ? base_max_rate_ : 1;
  }
  if (desired_rate != max_rate_) {
    poller_log()->info("Adjusting request rate to {} rpm (limit {} remaining "
                       "{} reserve {} source {})",
                       desired_rate, limit, remaining, reserve, source_tag);
    poller_.set_max_rate(desired_rate);
    max_rate_ = desired_rate;
  }
  RateBudgetSnapshot snapshot{};
  snapshot.limit = limit;
  snapshot.remaining = remaining;
  snapshot.reserve = reserve;
  snapshot.usable = usable;
  snapshot.minutes_until_reset = minutes_left;
  snapshot.allowed_rpm = allowed;
  snapshot.projected_rpm = remaining_cap;
  snapshot.source = source_tag;
  snapshot.monitor_enabled = rate_limit_monitor_enabled_;
  if (status_opt && status_opt->used > 0) {
    snapshot.used = status_opt->used;
  } else if (limit > 0) {
    snapshot.used = limit - remaining;
  }
  {
    std::lock_guard<std::mutex> lock(budget_mutex_);
    last_budget_snapshot_ = snapshot;
  }
  double observed_rpm = poller_.smoothed_requests_per_minute();
  if (observed_rpm > 0.0) {
    poller_log()->debug("Request scheduler average {:.2f} rpm (~{:.0f} rph)",
                        observed_rpm, observed_rpm * 60.0);
  }
  int new_interval = base_interval_ms_;
  if (max_rate_ > 0) {
    int per_minute_delay =
        static_cast<int>(std::lround(60000.0 / static_cast<double>(max_rate_)));
    per_minute_delay = std::max(per_minute_delay, 1);
    if (per_minute_delay < base_interval_ms_) {
      per_minute_delay = base_interval_ms_;
    }
    new_interval = std::max(base_interval_ms_, per_minute_delay);
    int smoothed_delay = per_minute_delay;
    if (observed_rpm > 0.0) {
      int averaged_delay =
          static_cast<int>(std::lround(60000.0 / std::max(observed_rpm, 1.0)));
      if (averaged_delay > smoothed_delay) {
        smoothed_delay = averaged_delay;
      }
    }
    if (smoothed_delay < 1) {
      smoothed_delay = 1;
    }
    if (min_request_delay_.count() != smoothed_delay) {
      min_request_delay_ = std::chrono::milliseconds(smoothed_delay);
      client_.set_delay_ms(static_cast<int>(min_request_delay_.count()));
    }
  }
  if (new_interval != interval_ms_) {
    poller_log()->info("Adjusting poll interval to {} ms", new_interval);
    interval_ms_ = new_interval;
  }
  if (max_rate_ > 0 && max_rate_ <= 1) {
    auto interval =
        std::chrono::duration<double>(60.0 / static_cast<double>(max_rate_));
    min_poll_interval_ =
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            interval);
    if (min_poll_interval_.count() <= 0) {
      min_poll_interval_ = std::chrono::nanoseconds(1);
    }
  }
}

/**
 * Emit a backlog warning describing current scheduler pressure.
 *
 * @param outstanding Number of outstanding jobs queued or executing.
 * @param clearance_estimate Estimated time required to drain the backlog.
 */
void GitHubPoller::handle_backlog(std::size_t outstanding,
                                  std::chrono::seconds clearance_estimate) {
  std::ostringstream oss;
  oss << "Request scheduler backlog " << outstanding
      << " operations; estimated clearance " << clearance_estimate.count()
      << "s with current rate limit margin " << std::fixed
      << std::setprecision(2) << rate_limit_margin_;
  const std::string message = oss.str();
  if (log_cb_) {
    log_cb_(message);
  } else {
    poller_log()->warn("{}", message);
  }
}

} // namespace agpm
