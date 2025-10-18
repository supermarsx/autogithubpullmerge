#include "app.hpp"
#include "demo_tui.hpp"
#include "github_client.hpp"
#include "github_poller.hpp"
#include "history.hpp"
#include "repo_discovery.hpp"
#include "tui.hpp"

#include <algorithm>
#include <iostream>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

int main(int argc, char **argv) {
  agpm::App app;
  int ret = app.run(argc, argv);
  if (ret != 0 || app.should_exit()) {
    return ret;
  }

  const auto &opts = app.options();
  const auto &cfg = app.config();
  if (opts.demo_tui) {
    return agpm::run_demo_tui();
  }

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
  const std::vector<std::string> &discovery_roots =
      !opts.repo_discovery_roots.empty() ? opts.repo_discovery_roots
                                         : cfg.repo_discovery_roots();
  auto parse_repo = [](const std::string &identifier,
                       std::pair<std::string, std::string> &out) {
    auto pos = identifier.find('/');
    if (pos == std::string::npos || pos == 0 || pos + 1 >= identifier.size()) {
      return false;
    }
    out.first = identifier.substr(0, pos);
    out.second = identifier.substr(pos + 1);
    return true;
  };
  auto repo_to_string = [](const std::pair<std::string, std::string> &repo) {
    return repo.first + "/" + repo.second;
  };
  auto build_filter = [&](const std::vector<std::string> &list, const char *label,
                          std::unordered_set<std::string> &out) -> bool {
    out.clear();
    out.reserve(list.size());
    for (const auto &entry : list) {
      std::pair<std::string, std::string> parsed;
      if (!parse_repo(entry, parsed)) {
        spdlog::error("Invalid repository identifier '{}' in {} list", entry, label);
        return false;
      }
      out.insert(repo_to_string(parsed));
    }
    return true;
  };
  std::vector<std::string> protected_branches =
      !opts.protected_branches.empty() ? opts.protected_branches
                                       : cfg.protected_branches();
  std::vector<std::string> protected_branch_excludes =
      !opts.protected_branch_excludes.empty()
          ? opts.protected_branch_excludes
          : cfg.protected_branch_excludes();
  std::unordered_set<std::string> include_set;
  if (!build_filter(include, "include", include_set)) {
    return 1;
  }
  std::unordered_set<std::string> exclude_set;
  if (!build_filter(exclude, "exclude", exclude_set)) {
    return 1;
  }

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
  curl_off_t download_limit =
      opts.download_limit != 0
          ? static_cast<curl_off_t>(opts.download_limit)
          : static_cast<curl_off_t>(cfg.download_limit());
  curl_off_t upload_limit = opts.upload_limit != 0
                                ? static_cast<curl_off_t>(opts.upload_limit)
                                : static_cast<curl_off_t>(cfg.upload_limit());
  curl_off_t max_download = opts.max_download != 0
                                ? static_cast<curl_off_t>(opts.max_download)
                                : static_cast<curl_off_t>(cfg.max_download());
  curl_off_t max_upload = opts.max_upload != 0
                              ? static_cast<curl_off_t>(opts.max_upload)
                              : static_cast<curl_off_t>(cfg.max_upload());
  std::string http_proxy =
      !opts.http_proxy.empty() ? opts.http_proxy : cfg.http_proxy();
  std::string https_proxy =
      !opts.https_proxy.empty() ? opts.https_proxy : cfg.https_proxy();
  auto http_client = std::make_unique<agpm::CurlHttpClient>(
      http_timeout * 1000, download_limit, upload_limit, max_download,
      max_upload, http_proxy, https_proxy);
  agpm::GitHubClient client(tokens, std::move(http_client), include_set,
                            exclude_set, delay_ms, http_timeout * 1000,
                            http_retries, api_base, opts.dry_run);
  bool allow_delete_base_branch =
      opts.allow_delete_base_branch || cfg.allow_delete_base_branch();
  client.set_allow_delete_base_branch(allow_delete_base_branch);
  agpm::GitHubGraphQLClient graphql_client(tokens, http_timeout * 1000,
                                           api_base);

  // Testing-only: perform a single HTTP request for open PRs and exit
  if (!opts.single_open_prs_repo.empty()) {
    auto prs = client.list_open_pull_requests_single(opts.single_open_prs_repo);
    std::size_t count = prs.size();
    for (const auto &pr : prs) {
      // Simple, stable output for tests
      std::cout << pr.owner << "/" << pr.repo << " #" << pr.number << ": "
                << pr.title << "\n";
    }
    std::cout << opts.single_open_prs_repo << " pull requests: " << count << "\n";
    return 0;
  }

  // Testing-only: perform a single HTTP request for branches and exit
  if (!opts.single_branches_repo.empty()) {
    auto branches = client.list_branches_single(opts.single_branches_repo);
    auto pos = opts.single_branches_repo.find('/');
    std::string owner = pos == std::string::npos
                            ? opts.single_branches_repo
                            : opts.single_branches_repo.substr(0, pos);
    std::string repo = pos == std::string::npos
                           ? std::string{}
                           : opts.single_branches_repo.substr(pos + 1);
    std::string repo_name = owner + (repo.empty() ? "" : "/" + repo);
    std::size_t count = branches.size();
    for (const auto &b : branches) {
      std::cout << repo_name << " branch: " << b << "\n";
    }
    std::cout << repo_name << " branches: " << count << "\n";
    return 0;
  }

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
  bool delete_stray = opts.delete_stray || cfg.delete_stray();
  bool auto_merge = opts.auto_merge || cfg.auto_merge();
  bool purge_only = opts.purge_only || cfg.purge_only();
  std::string sort_mode = !opts.sort.empty() ? opts.sort : cfg.sort_mode();
  int workers = opts.workers == 0 ? cfg.workers() : opts.workers;
  if (workers <= 0)
    workers = 1;

  std::vector<std::pair<std::string, std::string>> repos;
  if (opts.repo_discovery_mode == agpm::RepoDiscoveryMode::Disabled) {
    if (include.empty()) {
      spdlog::error(
          "Repository discovery disabled but no repositories specified via --include or config");
      return 1;
    }
    repos.reserve(include.size());
    for (const auto &r : include) {
      std::pair<std::string, std::string> parsed;
      if (!parse_repo(r, parsed)) {
        spdlog::error("Invalid repository identifier '{}'; expected OWNER/REPO", r);
        return 1;
      }
      if (!exclude_set.empty() &&
          exclude_set.find(repo_to_string(parsed)) != exclude_set.end()) {
        continue;
      }
      repos.push_back(std::move(parsed));
    }
    if (repos.empty()) {
      spdlog::error(
          "No repositories remain after applying include/exclude filters with discovery disabled");
      return 1;
    }
  } else if (agpm::repo_discovery_uses_tokens(opts.repo_discovery_mode)) {
    repos = client.list_repositories();
    if (!include_set.empty()) {
      std::vector<std::pair<std::string, std::string>> filtered;
      filtered.reserve(repos.size());
      for (const auto &repo : repos) {
        if (include_set.find(repo_to_string(repo)) != include_set.end()) {
          filtered.push_back(repo);
        }
      }
      repos.swap(filtered);
    }
    if (!exclude_set.empty() && !repos.empty()) {
      repos.erase(std::remove_if(repos.begin(), repos.end(),
                                 [&](const auto &repo) {
                                   return exclude_set.find(repo_to_string(repo)) !=
                                          exclude_set.end();
                                 }),
                  repos.end());
    }
    if (repos.empty()) {
      spdlog::warn("Repository discovery returned no repositories after filters");
    }
  } else if (agpm::repo_discovery_uses_filesystem(opts.repo_discovery_mode)) {
    if (discovery_roots.empty()) {
      spdlog::error(
          "Filesystem discovery requires at least one --repo-discovery-root or config entry");
      return 1;
    }
    repos = agpm::discover_repositories_from_filesystem(discovery_roots);
    if (!include_set.empty() && !repos.empty()) {
      std::vector<std::pair<std::string, std::string>> filtered;
      filtered.reserve(repos.size());
      for (const auto &repo : repos) {
        if (include_set.find(repo_to_string(repo)) != include_set.end()) {
          filtered.push_back(repo);
        }
      }
      repos.swap(filtered);
    }
    if (!exclude_set.empty() && !repos.empty()) {
      repos.erase(std::remove_if(repos.begin(), repos.end(),
                                 [&](const auto &repo) {
                                   return exclude_set.find(repo_to_string(repo)) !=
                                          exclude_set.end();
                                 }),
                  repos.end());
    }
    if (repos.empty()) {
      spdlog::warn("Filesystem discovery located no repositories after filters");
    }
  } else {
    spdlog::error("Unsupported repository discovery mode");
    return 1;
  }

  std::string history_db =
      !opts.history_db.empty() ? opts.history_db : cfg.history_db();
  agpm::PullRequestHistory history(history_db);

  agpm::GitHubPoller poller(
      client, repos, interval_ms, max_rate, workers, only_poll_prs,
      only_poll_stray, reject_dirty, purge_prefix, auto_merge, purge_only,
      sort_mode, &history, protected_branches, protected_branch_excludes,
      opts.dry_run,
      (opts.use_graphql || cfg.use_graphql()) ? &graphql_client : nullptr,
      delete_stray);

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
  agpm::Tui ui(client, poller, opts.log_limit);
  bool hotkeys_enabled = cfg.hotkeys_enabled();
  if (opts.hotkeys_explicit) {
    hotkeys_enabled = opts.hotkeys_enabled;
  }
  ui.set_hotkeys_enabled(hotkeys_enabled);
  const auto &hotkey_overrides = cfg.hotkey_bindings();
  if (!hotkey_overrides.empty()) {
    ui.configure_hotkeys(hotkey_overrides);
  }
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
  return 0;
}
