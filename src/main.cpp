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

    std::vector<std::string> tokens;
    if (!opts.api_keys.empty()) {
      tokens = opts.api_keys;
    } else if (!cfg.api_keys().empty()) {
      tokens = cfg.api_keys();
    }

    std::vector<std::string> include =
        !opts.include_repos.empty() ? opts.include_repos : cfg.include_repos();
    std::vector<std::string> exclude =
        !opts.exclude_repos.empty() ? opts.exclude_repos : cfg.exclude_repos();
    std::vector<std::string> protected_branches =
        !opts.protected_branches.empty() ? opts.protected_branches
                                         : cfg.protected_branches();
    std::vector<std::string> protected_branch_excludes =
        !opts.protected_branch_excludes.empty()
            ? opts.protected_branch_excludes
            : cfg.protected_branch_excludes();
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
    std::string api_base =
        !opts.api_base.empty() ? opts.api_base : cfg.api_base();
    agpm::GitHubClient client(tokens, nullptr, include_set, exclude_set,
                              delay_ms, http_timeout * 1000, http_retries,
                              api_base, opts.dry_run);

    int required_approvals = opts.required_approvals != 0
                                 ? opts.required_approvals
                                 : cfg.required_approvals();
    bool require_status_success =
        opts.require_status_success || cfg.require_status_success();
    bool require_mergeable_state =
        opts.require_mergeable_state || cfg.require_mergeable_state();
    client.set_required_approvals(required_approvals);
    client.set_require_status_success(require_status_success);
    client.set_require_mergeable_state(require_mergeable_state);

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
    int workers = opts.workers != 0 ? opts.workers : cfg.workers();
    if (workers <= 0)
      workers = 1;

    std::vector<std::pair<std::string, std::string>> repos;
    for (const auto &r : include) {
      auto pos = r.find('/');
      if (pos != std::string::npos) {
        repos.emplace_back(r.substr(0, pos), r.substr(pos + 1));
      }
    }
    if (repos.empty()) {
      repos = client.list_repositories();
    }

    std::string history_db =
        !opts.history_db.empty() ? opts.history_db : cfg.history_db();
    agpm::PullRequestHistory history(history_db);

    agpm::GitHubPoller poller(client, repos, interval_ms, max_rate, workers,
                              only_poll_prs, only_poll_stray, reject_dirty,
                              purge_prefix, auto_merge, purge_only, sort_mode,
                              &history, protected_branches,
                              protected_branch_excludes, opts.dry_run);

    if (!opts.export_csv.empty() || !opts.export_json.empty()) {
      poller.set_export_callback([&history, &opts]() {
        if (!opts.export_csv.empty()) {
          history.export_csv(opts.export_csv);
        }
        if (!opts.export_json.empty()) {
          history.export_json(opts.export_json);
        }
      });
    }
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
