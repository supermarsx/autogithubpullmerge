#include "app.hpp"
#include "github_client.hpp"
#include "github_poller.hpp"
#include "history.hpp"
#include "tui.hpp"

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

int main(int argc, char **argv) {
  agpm::App app;
  int ret = app.run(argc, argv);
  if (ret == 0) {
    const auto &opts = app.options();
    const auto &cfg = app.config();

    std::string token;
    if (!opts.api_keys.empty()) {
      token = opts.api_keys.front();
    } else if (!cfg.api_keys().empty()) {
      token = cfg.api_keys().front();
    }

    std::vector<std::string> include =
        !opts.include_repos.empty() ? opts.include_repos : cfg.include_repos();
    std::vector<std::string> exclude =
        !opts.exclude_repos.empty() ? opts.exclude_repos : cfg.exclude_repos();
    std::unordered_set<std::string> include_set(include.begin(), include.end());
    std::unordered_set<std::string> exclude_set(exclude.begin(), exclude.end());

    int max_rate = opts.max_request_rate != 60 ? opts.max_request_rate
                                               : cfg.max_request_rate();
    if (max_rate <= 0)
      max_rate = 60;
    int delay_ms = max_rate > 0 ? 60000 / max_rate : 0;

    int http_timeout =
        opts.http_timeout != 30 ? opts.http_timeout : cfg.http_timeout();
    int http_retries =
        opts.http_retries != 3 ? opts.http_retries : cfg.http_retries();
    agpm::GitHubClient client(token, nullptr, include_set, exclude_set,
                              delay_ms, http_timeout * 1000, http_retries);

    int interval =
        opts.poll_interval != 0 ? opts.poll_interval : cfg.poll_interval();
    int interval_ms = interval * 1000;

    bool only_poll_prs = opts.only_poll_prs || cfg.only_poll_prs();
    bool only_poll_stray = opts.only_poll_stray || cfg.only_poll_stray();
    bool reject_dirty = opts.reject_dirty || cfg.reject_dirty();
    std::string purge_prefix =
        !opts.purge_prefix.empty() ? opts.purge_prefix : cfg.purge_prefix();
    bool auto_merge = opts.auto_merge || cfg.auto_merge();
    bool purge_only = opts.purge_only || cfg.purge_only();
    std::string sort_mode = !opts.sort.empty() ? opts.sort : cfg.sort_mode();

    std::vector<std::pair<std::string, std::string>> repos;
    for (const auto &r : include) {
      auto pos = r.find('/');
      if (pos != std::string::npos) {
        repos.emplace_back(r.substr(0, pos), r.substr(pos + 1));
      }
    }

    std::string history_db =
        !opts.history_db.empty() ? opts.history_db : cfg.history_db();
    agpm::PullRequestHistory history(history_db);
    (void)history;

    agpm::GitHubPoller poller(client, repos, interval_ms, max_rate,
                              only_poll_prs, only_poll_stray, reject_dirty,
                              purge_prefix, auto_merge, purge_only, sort_mode);
    agpm::Tui ui(client, poller);
    poller.start();
    try {
      ui.init();
      ui.run();
    } catch (...) {
      poller.stop();
      ui.cleanup();
      throw;
    }
    poller.stop();
    ui.cleanup();
  }
  return ret;
}
