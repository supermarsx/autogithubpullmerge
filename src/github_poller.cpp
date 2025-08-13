#include "github_poller.hpp"

namespace agpm {

GitHubPoller::GitHubPoller(
    GitHubClient &client,
    std::vector<std::pair<std::string, std::string>> repos, int interval_ms,
    int max_rate, bool only_poll_prs, bool only_poll_stray, bool reject_dirty,
    std::string purge_prefix, bool auto_merge)
    : client_(client), repos_(std::move(repos)),
      poller_([this] { poll(); }, interval_ms, max_rate),
      only_poll_prs_(only_poll_prs), only_poll_stray_(only_poll_stray),
      reject_dirty_(reject_dirty), purge_prefix_(std::move(purge_prefix)),
      auto_merge_(auto_merge) {}

void GitHubPoller::start() { poller_.start(); }

void GitHubPoller::stop() { poller_.stop(); }

void GitHubPoller::poll() {
  for (const auto &r : repos_) {
    if (!only_poll_stray_) {
      auto prs = client_.list_pull_requests(r.first, r.second);
      if (auto_merge_) {
        for (const auto &pr : prs) {
          client_.merge_pull_request(r.first, r.second, pr.number);
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
}

} // namespace agpm
