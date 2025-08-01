#include "app.hpp"
#include "cli.hpp"
#include "config.hpp"
#include "history.hpp"
#include "log.hpp"
#include <cstdlib>
#include <spdlog/spdlog.h>

namespace agpm {

int App::run(int argc, char **argv) {
  options_ = parse_cli(argc, argv);
  include_repos_ = options_.include_repos;
  exclude_repos_ = options_.exclude_repos;
  spdlog::level::level_enum lvl =
      options_.verbose ? spdlog::level::debug : spdlog::level::info;
  try {
    lvl = spdlog::level::from_str(options_.log_level);
  } catch (const spdlog::spdlog_ex &) {
    // keep default
  }
  init_logger(lvl);
  if (!options_.config_file.empty()) {
    config_ = Config::from_file(options_.config_file);
  }
  PullRequestHistory history(options_.history_db);
  (void)history;
  if (options_.verbose) {
    spdlog::debug("Verbose mode enabled");
  }
  spdlog::info("Running agpm app");

  // Application logic goes here
  return 0;
}

} // namespace agpm
