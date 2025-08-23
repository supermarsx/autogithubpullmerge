#include "github_poller.hpp"
#include "sort.hpp"
#include <future>
#include <mutex>
#include <spdlog/spdlog.h>

namespace agpm {

GitHubPoller::GitHubPoller(
    GitHubClient &client,
    std::vector<std::pair<std::string, std::string>> repos, int interval_ms,
    int max_rate, int workers, bool only_poll_prs, bool only_poll_stray,
    bool reject_dirty, std::string purge_prefix, bool auto_merge,
    bool purge_only, std::string sort_mode, PullRequestHistory *history,
    std::vector<std::string> protected_branches,
    std::vector<std::string> protected_branch_excludes)
    : client_(client), repos_(std::move(repos)), poller_(workers, max_rate),
      interval_ms_(interval_ms), only_poll_prs_(only_poll_prs),
      only_poll_stray_(only_poll_stray), reject_dirty_(reject_dirty),
      purge_prefix_(std::move(purge_prefix)), auto_merge_(auto_merge),
      purge_only_(purge_only), sort_mode_(std::move(sort_mode)),
      history_(history), protected_branches_(std::move(protected_branches)),
      protected_branch_excludes_(
          std::move(protected_branch_excludes)) {}

void GitHubPoller::start() {
  spdlog::info("Starting GitHub poller");
  poller_.start();
  running_ = true;
  thread_ = std::thread([this] {
    while (running_) {
      poll();
      std::this_thread::sleep_for(
          std::chrono::milliseconds(interval_ms_));
    }
  });
}

void GitHubPoller::stop() {
  spdlog::info("Stopping GitHub poller");
  running_ = false;
  if (thread_.joinable()) {
    thread_.join();
  }
  poller_.stop();
}

void GitHubPoller::poll_now() { poll(); }

void GitHubPoller::set_pr_callback(
    std::function<void(const std::vector<PullRequest> &)> cb) {
  pr_cb_ = std::move(cb);
}

void GitHubPoller::set_log_callback(
    std::function<void(const std::string &)> cb) {
  log_cb_ = std::move(cb);
}

void GitHubPoller::set_export_callback(std::function<void()> cb) {
  export_cb_ = std::move(cb);
}

void GitHubPoller::poll() {
  spdlog::debug("Polling repositories");
  std::vector<PullRequest> all_prs;
  std::mutex pr_mutex;
  std::mutex log_mutex;
  std::vector<std::future<void>> futures;
  for (const auto &repo : repos_) {
    futures.push_back(poller_.submit([this, repo, &all_prs, &pr_mutex,
                                      &log_mutex] {
      if (purge_only_) {
        spdlog::debug("purge_only set - skipping repo {}/{}", repo.first,
                      repo.second);
        if (!purge_prefix_.empty()) {
          client_.cleanup_branches(repo.first, repo.second, purge_prefix_,
                                   protected_branches_,
                                   protected_branch_excludes_);
        }
        return;
      }
      std::vector<PullRequest> prs;
      if (!only_poll_stray_) {
        prs = client_.list_pull_requests(repo.first, repo.second);
        {
          std::lock_guard<std::mutex> lk(pr_mutex);
          all_prs.insert(all_prs.end(), prs.begin(), prs.end());
          if (history_) {
            for (const auto &pr : prs) {
              history_->insert(pr.number, pr.title, pr.merged);
            }
          }
        }
        if (auto_merge_) {
          for (const auto &pr : prs) {
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
            } else if (log_cb_) {
              std::lock_guard<std::mutex> lk(log_mutex);
              log_cb_("PR #" + std::to_string(pr.number) +
                      " did not meet merge requirements");
            }
          }
        }
      }
      if (!only_poll_prs_) {
        auto branches = client_.list_branches(repo.first, repo.second);
        std::vector<std::string> stray;
        for (const auto &b : branches) {
          if (purge_prefix_.empty() || b.rfind(purge_prefix_, 0) != 0) {
            stray.push_back(b);
          }
        }
        if (log_cb_) {
          std::lock_guard<std::mutex> lk(log_mutex);
          log_cb_(repo.first + "/" + repo.second +
                  " stray branches: " + std::to_string(stray.size()));
        }
      }
      if (!purge_prefix_.empty()) {
        client_.cleanup_branches(repo.first, repo.second, purge_prefix_,
                                 protected_branches_,
                                 protected_branch_excludes_);
      }
      if (!only_poll_prs_ && reject_dirty_) {
        client_.close_dirty_branches(repo.first, repo.second,
                                     protected_branches_,
                                     protected_branch_excludes_);
      }
    }));
  }
  for (auto &f : futures) {
    f.get();
  }
  sort_pull_requests(all_prs, sort_mode_);
  if (pr_cb_) {
    pr_cb_(all_prs);
  }
  if (export_cb_) {
    spdlog::info("Running export callback");
    export_cb_();
  }
  if (log_cb_) {
    std::lock_guard<std::mutex> lk(log_mutex);
    log_cb_("Polled " + std::to_string(all_prs.size()) + " pull requests");
  }
}

} // namespace agpm
