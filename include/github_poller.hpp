#ifndef AUTOGITHUBPULLMERGE_GITHUB_POLLER_HPP
#define AUTOGITHUBPULLMERGE_GITHUB_POLLER_HPP

#include "github_client.hpp"
#include "poller.hpp"
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
               int interval_ms, int max_rate, bool poll_prs_only = false,
               bool poll_stray_only = false, bool auto_reject_dirty = false);

  /// Start polling in a background thread.
  void start();
  /// Stop polling.
  void stop();

private:
  void poll();

  GitHubClient &client_;
  std::vector<std::pair<std::string, std::string>> repos_;
  Poller poller_;
  bool poll_prs_only_;
  bool poll_stray_only_;
  bool auto_reject_dirty_;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_GITHUB_POLLER_HPP
