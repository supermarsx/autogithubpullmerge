#include "github_poller.hpp"

namespace agpm {

GitHubPoller::GitHubPoller(
    GitHubClient &client,
    std::vector<std::pair<std::string, std::string>> repos, int interval_ms,
    int max_rate, bool poll_prs_only, bool poll_stray_only,
    bool auto_reject_dirty, std::string purge_prefix)
    : client_(client), repos_(std::move(repos)),
      poller_([this] { poll(); }, interval_ms, max_rate),
      poll_prs_only_(poll_prs_only), poll_stray_only_(poll_stray_only),
      auto_reject_dirty_(auto_reject_dirty),
      purge_prefix_(std::move(purge_prefix)) {}

void GitHubPoller::start() { poller_.start(); }

void GitHubPoller::stop() { poller_.stop(); }

void GitHubPoller::poll() {
  for (const auto &r : repos_) {
    if (!poll_stray_only_) {
      client_.list_pull_requests(r.first, r.second);
      if (!purge_prefix_.empty()) {
        client_.cleanup_branches(r.first, r.second, purge_prefix_);
      }
    }
    if (!poll_prs_only_) {
      // Placeholder for stray branch polling logic
      if (auto_reject_dirty_) {
        client_.close_dirty_branches(r.first, r.second);
      }
    }
  }
}

} // namespace agpm
