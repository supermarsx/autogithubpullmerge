#include "github_poller.hpp"
#include "sort.hpp"

namespace agpm {

GitHubPoller::GitHubPoller(
    GitHubClient &client,
    std::vector<std::pair<std::string, std::string>> repos, int interval_ms,
    int max_rate, bool only_poll_prs, bool only_poll_stray, bool reject_dirty,
    std::string purge_prefix, bool auto_merge, bool purge_only,
    std::string sort_mode)
    : client_(client), repos_(std::move(repos)),
      poller_([this] { poll(); }, interval_ms, max_rate),
      only_poll_prs_(only_poll_prs), only_poll_stray_(only_poll_stray),
      reject_dirty_(reject_dirty), purge_prefix_(std::move(purge_prefix)),
      auto_merge_(auto_merge), purge_only_(purge_only),
      sort_mode_(std::move(sort_mode)) {}

void GitHubPoller::start() { poller_.start(); }

void GitHubPoller::stop() { poller_.stop(); }

void GitHubPoller::poll_now() { poll(); }

void GitHubPoller::set_pr_callback(
    std::function<void(const std::vector<PullRequest> &)> cb) {
  pr_cb_ = std::move(cb);
}

void GitHubPoller::set_log_callback(
    std::function<void(const std::string &)> cb) {
  log_cb_ = std::move(cb);
}

void GitHubPoller::poll() {
  std::vector<PullRequest> all_prs;
  for (const auto &r : repos_) {
    if (purge_only_) {
      if (!purge_prefix_.empty()) {
        client_.cleanup_branches(r.first, r.second, purge_prefix_);
      }
      continue;
    }
    if (!only_poll_stray_) {
      auto prs = client_.list_pull_requests(r.first, r.second);
      all_prs.insert(all_prs.end(), prs.begin(), prs.end());
      if (auto_merge_) {
        for (const auto &pr : prs) {
          client_.merge_pull_request(pr.owner, pr.repo, pr.number);
        }
      }
      if (!purge_prefix_.empty()) {
        client_.cleanup_branches(r.first, r.second, purge_prefix_);
      }
    }
    if (!only_poll_prs_) {
      // Placeholder for stray branch polling logic
      if (reject_dirty_) {
        client_.close_dirty_branches(r.first, r.second);
      }
    }
  }
  sort_pull_requests(all_prs, sort_mode_);
  if (pr_cb_) {
    pr_cb_(all_prs);
  }
  if (log_cb_) {
    log_cb_("Polled " + std::to_string(all_prs.size()) + " pull requests");
  }
}

} // namespace agpm
