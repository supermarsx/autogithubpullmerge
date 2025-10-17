#include "app.hpp"
#include "cli.hpp"
#include "config.hpp"
#include "log.hpp"
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <spdlog/spdlog.h>

#if defined(_WIN32)
#include <shellapi.h>
#include <windows.h>
#else
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
extern char **environ;
#endif

namespace {

bool open_url_in_browser(const std::string &url) {
  if (const char *skip = std::getenv("AGPM_TEST_SKIP_BROWSER")) {
    if (*skip != '\0' && *skip != '0') {
      return true;
    }
  }
#if defined(_WIN32)
  HINSTANCE res = ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr,
                                SW_SHOWNORMAL);
  auto code = reinterpret_cast<uintptr_t>(res);
  if (code > 32) {
    return true;
  }
  spdlog::debug("ShellExecuteA failed with code {}", code);
  return false;
#else
#if defined(__APPLE__)
  const char *program = "open";
#else
  const char *program = "xdg-open";
#endif
  std::string program_arg(program);
  std::string url_arg = url;
  char *argv[] = {program_arg.data(), url_arg.data(), nullptr};
  pid_t pid = -1;
  int spawn_res =
      posix_spawnp(&pid, program_arg.c_str(), nullptr, nullptr, argv, environ);
  if (spawn_res != 0) {
    spdlog::debug("posix_spawnp failed for {}: {}", program,
                  std::strerror(spawn_res));
    return false;
  }
  int status = 0;
  if (waitpid(pid, &status, 0) != pid) {
    return false;
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

bool write_pat_to_file(const std::string &path_str, const std::string &pat) {
  namespace fs = std::filesystem;
  fs::path path(path_str);
  std::error_code ec;
  if (path.has_parent_path()) {
    fs::create_directories(path.parent_path(), ec);
    if (ec) {
      spdlog::error("Failed to create directories for {}: {}", path_str,
                    ec.message());
      return false;
    }
  }
  std::ofstream out(path, std::ios::out | std::ios::trunc);
  if (!out) {
    spdlog::error("Failed to open {} for writing", path_str);
    return false;
  }
  out << pat << '\n';
  out.close();
  if (!out) {
    spdlog::error("Failed to write personal access token to {}", path_str);
    return false;
  }
#ifndef _WIN32
  ::chmod(path.c_str(), S_IRUSR | S_IWUSR);
#endif
  return true;
}

} // namespace

namespace agpm {

int App::run(int argc, char **argv) {
  should_exit_ = false;
  try {
    options_ = parse_cli(argc, argv);
  } catch (const CliParseExit &exit) {
    should_exit_ = true;
    return exit.exit_code();
  } catch (const std::exception &e) {
    spdlog::error("{}", e.what());
    should_exit_ = true;
    return 1;
  }
  include_repos_ = options_.include_repos;
  exclude_repos_ = options_.exclude_repos;
  if (!options_.config_file.empty()) {
    config_ = Config::from_file(options_.config_file);
  }
  if (options_.hotkeys_explicit) {
    config_.set_hotkeys_enabled(options_.hotkeys_enabled);
  }
  options_.assume_yes = options_.assume_yes || config_.assume_yes();
  options_.dry_run = options_.dry_run || config_.dry_run();
  if (options_.log_limit == 200) {
    options_.log_limit = config_.log_limit();
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
      spdlog::error("Operation cancelled by user");
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
  init_logger(lvl, pattern, log_file);
  if (options_.verbose) {
    spdlog::debug("Verbose mode enabled");
  }
  if (options_.dry_run) {
    spdlog::info("Dry run mode enabled");
  }
  if (options_.open_pat_window) {
    const std::string url = "https://github.com/settings/tokens/new";
    bool ok = open_url_in_browser(url);
    if (ok) {
      spdlog::info("Opened GitHub PAT creation page");
    } else {
      spdlog::error("Failed to open GitHub PAT creation page");
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
      spdlog::error("No personal access token provided");
      should_exit_ = true;
      return 1;
    }
    bool ok = write_pat_to_file(options_.pat_save_path, pat_value);
    if (ok) {
      spdlog::info("Personal access token saved to {}", options_.pat_save_path);
    } else {
      spdlog::error("Failed to persist personal access token");
    }
    should_exit_ = true;
    return ok ? 0 : 1;
  }
  spdlog::info("Running agpm app");

  // Application logic goes here
  return 0;
}

} // namespace agpm
