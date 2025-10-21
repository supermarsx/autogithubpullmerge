#include "github_poller.hpp"
#include "log.hpp"
#include "sort.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <future>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <spdlog/spdlog.h>

namespace agpm {

namespace {
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
 */
GitHubPoller::GitHubPoller(
    GitHubClient &client,
    std::vector<std::pair<std::string, std::string>> repos, int interval_ms,
    int max_rate, int workers, bool only_poll_prs, bool only_poll_stray,
    bool reject_dirty, std::string purge_prefix, bool auto_merge,
    bool purge_only, std::string sort_mode, PullRequestHistory *history,
    std::vector<std::string> protected_branches,
    std::vector<std::string> protected_branch_excludes, bool dry_run,
    GitHubGraphQLClient *graphql_client, bool delete_stray,
    double rate_limit_margin)
    : client_(client), repos_(std::move(repos)), poller_(workers, max_rate),
      interval_ms_(interval_ms), base_interval_ms_(interval_ms),
      max_rate_(max_rate), base_max_rate_(max_rate),
      only_poll_prs_(only_poll_prs), only_poll_stray_(only_poll_stray),
      reject_dirty_(reject_dirty), delete_stray_(delete_stray),
      purge_prefix_(std::move(purge_prefix)), auto_merge_(auto_merge),
      purge_only_(purge_only), sort_mode_(std::move(sort_mode)),
      dry_run_(dry_run), graphql_client_(graphql_client),
      protected_branches_(std::move(protected_branches)),
      protected_branch_excludes_(std::move(protected_branch_excludes)),
      history_(history), rate_limit_margin_(rate_limit_margin) {
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
  bool skip_branch_ops = only_poll_prs_ || (max_rate_ > 0 && max_rate_ <= 1);
  poller_log()->debug("Polling repositories");
  std::vector<PullRequest> all_prs;
  std::mutex pr_mutex;
  std::mutex log_mutex;
  std::atomic<std::size_t> total_pr_count{0};
  std::atomic<std::size_t> total_branch_count{0};
  std::vector<std::future<void>> futures;
  futures.reserve(repos_.size());
  for (const auto &repo : repos_) {
    futures.emplace_back(poller_.submit(
        [this, repo, skip_branch_ops, &all_prs, &pr_mutex, &log_mutex,
         &total_pr_count, &total_branch_count] {
      if (purge_only_) {
        poller_log()->debug("purge_only set - skipping repo {}/{}", repo.first,
                      repo.second);
        if (!purge_prefix_.empty()) {
          client_.cleanup_branches(repo.first, repo.second, purge_prefix_,
                                   protected_branches_,
                                   protected_branch_excludes_);
          if (notifier_) {
            notifier_->notify("Purged branches in " + repo.first + "/" +
                              repo.second);
          }
        }
        return;
      }
      if (!only_poll_stray_) {
        const std::vector<PullRequest> prs = [this, &repo]() {
          if (graphql_client_) {
            return graphql_client_->list_pull_requests(repo.first,
                                                       repo.second);
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
          log_cb_(repo.first + "/" + repo.second + " pull requests: " +
                  std::to_string(prs.size()));
        } else {
          poller_log()->info("Fetched {} pull requests for {}/{}", prs.size(),
                       repo.first, repo.second);
        }
        if (auto_merge_) {
          for (const auto &pr : prs) {
            if (dry_run_) {
              client_.merge_pull_request(pr.owner, pr.repo, pr.number);
              if (log_cb_) {
                std::lock_guard<std::mutex> lk(log_mutex);
                log_cb_("Would merge PR #" + std::to_string(pr.number));
              }
              continue;
            }
            bool merged =
                client_.merge_pull_request(pr.owner, pr.repo, pr.number);
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
            } else if (log_cb_) {
              std::lock_guard<std::mutex> lk(log_mutex);
              log_cb_("PR #" + std::to_string(pr.number) +
                      " did not meet merge requirements");
            }
          }
        }
      }
      if (!skip_branch_ops) {
        auto branches = client_.list_branches(repo.first, repo.second);
        total_branch_count.fetch_add(branches.size(),
                                     std::memory_order_relaxed);
        if (log_cb_) {
          std::lock_guard<std::mutex> lk(log_mutex);
          log_cb_(repo.first + "/" + repo.second + " branches: " +
                  std::to_string(branches.size()));
        } else {
          poller_log()->info("Fetched {} branches for {}/{}", branches.size(),
                       repo.first, repo.second);
        }
        std::vector<std::string> stray;
        std::copy_if(branches.begin(), branches.end(),
                     std::back_inserter(stray),
                     [this](const std::string &branch) {
                       return purge_prefix_.empty() ||
                              branch.rfind(purge_prefix_, 0) != 0;
                     });
        if (log_cb_) {
          std::lock_guard<std::mutex> lk(log_mutex);
          log_cb_(repo.first + "/" + repo.second +
                  " stray branches: " + std::to_string(stray.size()));
        } else {
          poller_log()->info("{} / {} stray branches: {}", repo.first, repo.second,
                       stray.size());
        }
        if (delete_stray_) {
          for (const auto &b : stray) {
            client_.cleanup_branches(repo.first, repo.second, b,
                                     protected_branches_,
                                     protected_branch_excludes_);
          }
        }
      }
      if (!purge_prefix_.empty()) {
        client_.cleanup_branches(repo.first, repo.second, purge_prefix_,
                                 protected_branches_,
                                 protected_branch_excludes_);
        if (notifier_) {
          notifier_->notify("Purged branches in " + repo.first + "/" +
                            repo.second);
        }
      }
      if (!skip_branch_ops && reject_dirty_) {
        client_.close_dirty_branches(repo.first, repo.second,
                                     protected_branches_,
                                     protected_branch_excludes_);
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
  sort_pull_requests(all_prs, sort_mode_);
  if (pr_cb_) {
    pr_cb_(all_prs);
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
  auto status_opt = client_.rate_limit_status();
  if (!status_opt) {
    return;
  }
  const auto &status = *status_opt;
  if (status.limit <= 0) {
    return;
  }
  long reserve = static_cast<long>(
      std::floor(static_cast<double>(status.limit) * rate_limit_margin_));
  if (reserve < 0)
    reserve = 0;
  if (reserve > status.limit)
    reserve = status.limit;
  long usable = status.remaining - reserve;
  if (usable < 0)
    usable = 0;
  double seconds_left =
      std::max<double>(status.reset_after.count(), 60.0);
  double minutes_left = seconds_left / 60.0;
  double hourly_cap =
      static_cast<double>(status.limit) * (1.0 - rate_limit_margin_) / 60.0;
  if (hourly_cap <= 0.0) {
    hourly_cap = static_cast<double>(status.limit) / 60.0;
  }
  double remaining_cap = minutes_left > 0.0
                             ? static_cast<double>(usable) / minutes_left
                             : static_cast<double>(usable);
  double allowed = hourly_cap;
  if (remaining_cap > 0.0) {
    allowed = std::min(allowed, remaining_cap);
  }
  if (base_max_rate_ > 0) {
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
    poller_log()->info(
        "Adjusting request rate to {} rpm (limit {} remaining {} reserve {})",
        desired_rate, status.limit, status.remaining, reserve);
    poller_.set_max_rate(desired_rate);
    max_rate_ = desired_rate;
  }
  int new_interval = base_interval_ms_;
  if (max_rate_ > 0) {
    int per_minute_delay = static_cast<int>(
        std::lround(60000.0 / static_cast<double>(max_rate_)));
    per_minute_delay = std::max(per_minute_delay, 1);
    if (per_minute_delay < base_interval_ms_) {
      per_minute_delay = base_interval_ms_;
    }
    new_interval = std::max(base_interval_ms_, per_minute_delay);
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

} // namespace agpm
