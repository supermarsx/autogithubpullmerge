#include "app.hpp"
#include "cli.hpp"
#include "config.hpp"
#include "log.hpp"
#include "pat.hpp"
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>

namespace agpm {

namespace {
std::shared_ptr<spdlog::logger> app_log() {
  static auto logger = [] {
    ensure_default_logger();
    return category_logger("app");
  }();
  return logger;
}
} // namespace

/**
 * Execute the main application flow.
 *
 * This routine orchestrates CLI parsing, configuration loading, logger
 * initialization, and optional destructive confirmation prompts. It also
 * handles personal access token interactions.
 *
 * @param argc Argument count passed from @c main().
 * @param argv Argument vector passed from @c main().
 * @return Zero on success, non-zero if execution should terminate with an
 *         error code.
 */
int App::run(int argc, char **argv) {
  should_exit_ = false;
  try {
    options_ = parse_cli(argc, argv);
  } catch (const CliParseExit &exit) {
    should_exit_ = true;
    return exit.exit_code();
  } catch (const std::exception &e) {
    app_log()->error("{}", e.what());
    should_exit_ = true;
    return 1;
  }
  include_repos_ = options_.include_repos;
  exclude_repos_ = options_.exclude_repos;
  if (!options_.config_file.empty()) {
    config_ = Config::from_file(options_.config_file);
  }
  if (!options_.repo_discovery_explicit) {
    options_.repo_discovery_mode = config_.repo_discovery_mode();
  } else {
    config_.set_repo_discovery_mode(options_.repo_discovery_mode);
  }
  if (options_.repo_discovery_roots.empty()) {
    options_.repo_discovery_roots = config_.repo_discovery_roots();
  }
  if (options_.hotkeys_explicit) {
    config_.set_hotkeys_enabled(options_.hotkeys_enabled);
  }
  options_.assume_yes = options_.assume_yes || config_.assume_yes();
  options_.dry_run = options_.dry_run || config_.dry_run();
  if (options_.log_limit == 200) {
    options_.log_limit = config_.log_limit();
  }
  if (!options_.log_rotate_explicit) {
    options_.log_rotate = config_.log_rotate();
  }
  if (!options_.log_compress_explicit) {
    options_.log_compress = config_.log_compress();
  }
  if (!options_.log_categories_explicit) {
    options_.log_categories = config_.log_categories();
  } else {
    config_.set_log_categories(options_.log_categories);
  }
  if (!options_.log_sidecar_explicit) {
    options_.log_sidecar = config_.log_sidecar();
  } else {
    config_.set_log_sidecar(options_.log_sidecar);
  }
  if (!options_.rate_limit_margin_explicit) {
    options_.rate_limit_margin = config_.rate_limit_margin();
  } else {
    config_.set_rate_limit_margin(options_.rate_limit_margin);
  }
  options_.reject_dirty = options_.reject_dirty || config_.reject_dirty();
  options_.delete_stray = options_.delete_stray || config_.delete_stray();
  options_.allow_delete_base_branch =
      options_.allow_delete_base_branch || config_.allow_delete_base_branch();
  options_.auto_merge = options_.auto_merge || config_.auto_merge();
  if (options_.purge_prefix.empty()) {
    options_.purge_prefix = config_.purge_prefix();
  }
  options_.purge_only = options_.purge_only || config_.purge_only();
  options_.open_pat_window =
      options_.open_pat_window || config_.open_pat_page();
  if (options_.pat_save_path.empty()) {
    options_.pat_save_path = config_.pat_save_path();
  }
  if (options_.pat_value.empty()) {
    options_.pat_value = config_.pat_value();
  }
  if (options_.export_csv.empty()) {
    options_.export_csv = config_.export_csv();
  }
  if (options_.export_json.empty()) {
    options_.export_json = config_.export_json();
  }
  if (options_.single_open_prs_repo.empty()) {
    options_.single_open_prs_repo = config_.single_open_prs_repo();
  }
  if (options_.single_branches_repo.empty()) {
    options_.single_branches_repo = config_.single_branches_repo();
  }
  bool destructive =
      (options_.reject_dirty || options_.delete_stray ||
       options_.allow_delete_base_branch || options_.auto_merge ||
       !options_.purge_prefix.empty() || options_.purge_only) &&
      !options_.dry_run;
  if (destructive && !options_.assume_yes) {
    std::cout << "Destructive options enabled. Continue? [y/N]: ";
    std::string resp;
    std::getline(std::cin, resp);
    if (!(resp == "y" || resp == "Y" || resp == "yes" || resp == "YES")) {
      app_log()->error("Operation cancelled by user");
      should_exit_ = true;
      return 1;
    }
  }
  std::string level_str = options_.verbose ? "debug" : "info";
  if (options_.log_level != "info") {
    level_str = options_.log_level;
  } else if (config_.log_level() != "info") {
    level_str = config_.log_level();
  }
  spdlog::level::level_enum lvl = spdlog::level::info;
  try {
    lvl = spdlog::level::from_str(level_str);
  } catch (const spdlog::spdlog_ex &) {
    // keep default
  }
  std::string pattern = config_.log_pattern();
  std::string log_file = config_.log_file();
  if (!options_.log_file.empty()) {
    log_file = options_.log_file;
  }
  init_logger(lvl, pattern, log_file,
              static_cast<std::size_t>(options_.log_rotate),
              options_.log_compress);
  std::unordered_map<std::string, spdlog::level::level_enum> category_levels;
  for (const auto &[category, level_str] : options_.log_categories) {
    try {
      category_levels[category] = spdlog::level::from_str(level_str);
    } catch (const spdlog::spdlog_ex &) {
      app_log()->warn("Ignoring invalid log level '{}' for category '{}'",
                      level_str, category);
    }
  }
  configure_log_categories(category_levels);
  if (options_.verbose) {
    app_log()->debug("Verbose mode enabled");
  }
  if (options_.dry_run) {
    app_log()->info("Dry run mode enabled");
  }
  if (options_.open_pat_window) {
    bool ok = open_pat_creation_page();
    if (ok) {
      app_log()->info("Opened GitHub PAT creation page");
    } else {
      app_log()->error("Failed to open GitHub PAT creation page");
    }
    should_exit_ = true;
    return ok ? 0 : 1;
  }
  if (!options_.pat_save_path.empty()) {
    std::string pat_value = options_.pat_value;
    if (pat_value.empty()) {
      std::cout << "Enter personal access token: ";
      std::getline(std::cin, pat_value);
    }
    if (pat_value.empty()) {
      app_log()->error("No personal access token provided");
      should_exit_ = true;
      return 1;
    }
    bool ok = save_pat_to_file(options_.pat_save_path, pat_value);
    if (ok) {
      app_log()->info("Personal access token saved to {}",
                      options_.pat_save_path);
    } else {
      app_log()->error("Failed to persist personal access token");
    }
    should_exit_ = true;
    return ok ? 0 : 1;
  }
  app_log()->info("Running agpm app");

  // Application logic goes here
  return 0;
}

} // namespace agpm
