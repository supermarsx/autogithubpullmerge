#include "app.hpp"
#include "cli.hpp"
#include "config.hpp"
#include "log.hpp"
#include <cstdlib>
#include <exception>
#include <spdlog/spdlog.h>

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
  spdlog::info("Running agpm app");

  // Application logic goes here
  return 0;
}

} // namespace agpm
