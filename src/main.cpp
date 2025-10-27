#include "app.hpp"
#include "demo_tui.hpp"
#include "github_client.hpp"
#include "github_poller.hpp"
#include "history.hpp"
#include "hook.hpp"
#include "log.hpp"
#include "mcp_server.hpp"
#include "repo_discovery.hpp"
#include "tui.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

/**
 * Program entry point orchestrating configuration loading and UI startup.
 *
 * @param argc Number of CLI arguments received from the OS.
 * @param argv Null-terminated array containing the raw CLI arguments.
 * @return Process exit code forwarded from the application logic.
 */
namespace {
std::shared_ptr<spdlog::logger> main_log() {
  static auto logger = [] {
    agpm::ensure_default_logger();
    return agpm::category_logger("main");
  }();
  return logger;
}
} // namespace

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
  auto build_filter = [&](const std::vector<std::string> &list,
                          const char *label,
                          std::unordered_set<std::string> &out) -> bool {
    out.clear();
    out.reserve(list.size());
    for (const auto &entry : list) {
      std::pair<std::string, std::string> parsed;
      if (!parse_repo(entry, parsed)) {
        main_log()->error("Invalid repository identifier '{}' in {} list",
                          entry, label);
        return false;
      }
      out.insert(repo_to_string(parsed));
    }
    return true;
  };
  std::vector<std::string> protected_branches = !opts.protected_branches.empty()
                                                    ? opts.protected_branches
                                                    : cfg.protected_branches();
  std::vector<std::string> protected_branch_excludes =
      !opts.protected_branch_excludes.empty() ? opts.protected_branch_excludes
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
  int hourly_limit = opts.max_hourly_requests != 0 ? opts.max_hourly_requests
                                                   : cfg.max_hourly_requests();
  if (hourly_limit < 0)
    hourly_limit = 0;
  int delay_ms = max_rate > 0 ? 60000 / max_rate : 0;

  int http_timeout =
      opts.http_timeout != 30 ? opts.http_timeout : cfg.http_timeout();
  int http_retries =
      opts.http_retries != 3 ? opts.http_retries : cfg.http_retries();
  std::string api_base =
      !opts.api_base.empty() ? opts.api_base : cfg.api_base();
  curl_off_t download_limit =
      opts.download_limit != 0 ? static_cast<curl_off_t>(opts.download_limit)
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

  agpm::HookSettings hook_settings;
  std::shared_ptr<agpm::HookDispatcher> hook_dispatcher;
  if (opts.hooks_enabled) {
    hook_settings.enabled = true;
    hook_settings.pull_threshold = opts.hook_pull_threshold;
    hook_settings.branch_threshold = opts.hook_branch_threshold;
    if (!opts.hook_command.empty()) {
      agpm::HookAction cmd_action;
      cmd_action.type = agpm::HookActionType::Command;
      cmd_action.command = opts.hook_command;
      hook_settings.default_actions.push_back(cmd_action);
    }
    if (!opts.hook_endpoint.empty()) {
      agpm::HookAction http_action;
      http_action.type = agpm::HookActionType::Http;
      http_action.endpoint = opts.hook_endpoint;
      http_action.method =
          opts.hook_method.empty() ? std::string{"POST"} : opts.hook_method;
      for (const auto &header : opts.hook_headers) {
        http_action.headers.emplace_back(header.first, header.second);
      }
      hook_settings.default_actions.push_back(std::move(http_action));
    }
    hook_dispatcher = std::make_shared<agpm::HookDispatcher>(hook_settings);
  }

  // Testing-only: perform a single HTTP request for open PRs and exit
  if (!opts.single_open_prs_repo.empty()) {
    auto prs = client.list_open_pull_requests_single(opts.single_open_prs_repo);
    std::size_t count = prs.size();
    for (const auto &pr : prs) {
      // Simple, stable output for tests
      std::cout << pr.owner << "/" << pr.repo << " #" << pr.number << ": "
                << pr.title << "\n";
    }
    std::cout << opts.single_open_prs_repo << " pull requests: " << count
              << "\n";
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
  agpm::StrayDetectionMode stray_detection_mode = cfg.stray_detection_mode();
  if (opts.stray_detection_mode_explicit) {
    stray_detection_mode = opts.stray_detection_mode;
  }
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
  double rate_limit_margin = opts.rate_limit_margin;
  int rate_limit_refresh_interval = opts.rate_limit_refresh_interval;
  bool retry_rate_limit_endpoint = opts.retry_rate_limit_endpoint;
  int rate_limit_retry_limit = opts.rate_limit_retry_limit;

  std::vector<std::pair<std::string, std::string>> repos;
  if (opts.repo_discovery_mode == agpm::RepoDiscoveryMode::Disabled) {
    if (include.empty()) {
      main_log()->error("Repository discovery disabled but no repositories "
                        "specified via --include or config");
      return 1;
    }
    repos.reserve(include.size());
    for (const auto &r : include) {
      std::pair<std::string, std::string> parsed;
      if (!parse_repo(r, parsed)) {
        main_log()->error(
            "Invalid repository identifier '{}'; expected OWNER/REPO", r);
        return 1;
      }
      if (!exclude_set.empty() &&
          exclude_set.find(repo_to_string(parsed)) != exclude_set.end()) {
        continue;
      }
      repos.push_back(std::move(parsed));
    }
    if (repos.empty()) {
      main_log()->error("No repositories remain after applying include/exclude "
                        "filters with discovery disabled");
      return 1;
    }
  } else {
    const bool uses_tokens =
        agpm::repo_discovery_uses_tokens(opts.repo_discovery_mode);
    const bool uses_filesystem =
        agpm::repo_discovery_uses_filesystem(opts.repo_discovery_mode);

    if (uses_filesystem && discovery_roots.empty()) {
      main_log()->error("Filesystem discovery requires at least one "
                        "--repo-discovery-root or config entry");
      return 1;
    }

    std::unordered_set<std::string> seen_repos;
    auto append_unique =
        [&](const std::vector<std::pair<std::string, std::string>> &source) {
          for (const auto &repo : source) {
            auto key = repo_to_string(repo);
            if (seen_repos.insert(key).second) {
              repos.push_back(repo);
            }
          }
        };

    if (uses_tokens) {
      append_unique(client.list_repositories());
    }
    if (uses_filesystem) {
      append_unique(
          agpm::discover_repositories_from_filesystem(discovery_roots));
    }

    if (!include_set.empty() && !repos.empty()) {
      repos.erase(std::remove_if(repos.begin(), repos.end(),
                                 [&](const auto &repo) {
                                   return include_set.find(repo_to_string(
                                              repo)) == include_set.end();
                                 }),
                  repos.end());
    }

    if (!exclude_set.empty() && !repos.empty()) {
      repos.erase(std::remove_if(repos.begin(), repos.end(),
                                 [&](const auto &repo) {
                                   return exclude_set.find(repo_to_string(
                                              repo)) != exclude_set.end();
                                 }),
                  repos.end());
    }

    if (repos.empty()) {
      if (uses_tokens && uses_filesystem) {
        main_log()->warn("Combined repository discovery returned no "
                         "repositories after filters");
      } else if (uses_tokens) {
        main_log()->warn(
            "Repository discovery returned no repositories after filters");
      } else if (uses_filesystem) {
        main_log()->warn(
            "Filesystem discovery located no repositories after filters");
      } else {
        main_log()->warn("Repository discovery returned no repositories");
      }
    }
  }

  std::string history_db =
      !opts.history_db.empty() ? opts.history_db : cfg.history_db();
  agpm::PullRequestHistory history(history_db);

  agpm::GitHubPoller poller(
      client, repos, interval_ms, max_rate, hourly_limit, workers,
      only_poll_prs, only_poll_stray, stray_detection_mode, reject_dirty,
      purge_prefix, auto_merge, purge_only, sort_mode, &history,
      protected_branches, protected_branch_excludes, opts.dry_run,
      (opts.use_graphql || cfg.use_graphql()) ? &graphql_client : nullptr,
      delete_stray, rate_limit_margin,
      std::chrono::seconds(rate_limit_refresh_interval),
      retry_rate_limit_endpoint, rate_limit_retry_limit);

  if (hook_dispatcher) {
    poller.set_hook_dispatcher(hook_dispatcher);
    poller.set_hook_thresholds(hook_settings.pull_threshold,
                               hook_settings.branch_threshold);
  }

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
  std::unique_ptr<agpm::GitHubMcpBackend> mcp_backend;
  std::unique_ptr<agpm::McpServer> mcp_server;
  std::unique_ptr<agpm::McpServerRunner> mcp_runner;
  agpm::McpServerOptions mcp_options;
  if (opts.mcp_server_enabled) {
    mcp_options.bind_address = !opts.mcp_server_bind_address.empty()
                                   ? opts.mcp_server_bind_address
                                   : cfg.mcp_server_bind_address();
    mcp_options.port = opts.mcp_server_port > 0 ? opts.mcp_server_port
                                                : cfg.mcp_server_port();
    mcp_options.backlog = opts.mcp_server_backlog > 0
                              ? opts.mcp_server_backlog
                              : cfg.mcp_server_backlog();
    mcp_options.max_clients = opts.mcp_server_max_clients >= 0
                                  ? opts.mcp_server_max_clients
                                  : cfg.mcp_server_max_clients();
    mcp_backend = std::make_unique<agpm::GitHubMcpBackend>(
        client, repos, protected_branches, protected_branch_excludes);
    mcp_server = std::make_unique<agpm::McpServer>(*mcp_backend);
    std::string listen_host =
        mcp_options.bind_address.empty() ? std::string{"0.0.0.0"}
                                         : mcp_options.bind_address;
    std::string max_clients_desc =
        mcp_options.max_clients == 0
            ? std::string{"unlimited"}
            : std::to_string(mcp_options.max_clients);
    main_log()->info(
        "Starting MCP server on {}:{} (backlog {}, max clients {})",
        listen_host, mcp_options.port, mcp_options.backlog, max_clients_desc);
  }
  agpm::Tui ui(client, poller, opts.log_limit, opts.log_sidecar,
               opts.mcp_caddy_window, opts.request_caddy_window);
  std::function<void(const std::string &)> mcp_event_sink;
  if (opts.mcp_caddy_window) {
    mcp_event_sink = [&ui](const std::string &msg) { ui.add_mcp_event(msg); };
  }
  if (mcp_server) {
    if (mcp_event_sink) {
      mcp_server->set_event_callback(mcp_event_sink);
    }
    mcp_runner =
        std::make_unique<agpm::McpServerRunner>(*mcp_server, mcp_options);
    if (mcp_event_sink) {
      mcp_runner->set_event_sink(mcp_event_sink);
    }
  }
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
    if (mcp_runner) {
      mcp_runner->start();
    }
    ui.run();
  } catch (...) {
    if (mcp_runner) {
      mcp_runner->stop();
    }
    poller.stop();
    ui.cleanup();
    throw;
  }
  if (mcp_runner) {
    mcp_runner->stop();
  }
  poller.stop();
  ui.cleanup();
  return 0;
}
