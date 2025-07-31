#include "app.hpp"
#include "cli.hpp"
#include "config.hpp"
#include "github_client.hpp"
#include "log.hpp"
#include "tui.hpp"
#include <iostream>
#include <cstdlib>
#include <spdlog/spdlog.h>

namespace agpm {

int App::run(int argc, char **argv) {
  options_ = parse_cli(argc, argv);
  spdlog::level::level_enum lvl = spdlog::level::info;
  try {
    lvl = spdlog::level::from_str(options_.log_level);
  } catch (const spdlog::spdlog_ex &) {
    // keep default
  }
  init_logger(lvl);
  if (!options_.config_file.empty()) {
    config_ = Config::from_file(options_.config_file);
  }
  if (options_.verbose) {
    std::cout << "Verbose mode enabled" << std::endl;
  }
  std::cout << "Running agpm app" << std::endl;

  if (argc == 1) {
    const char *token = std::getenv("GITHUB_TOKEN");
    const char *owner = std::getenv("GITHUB_OWNER");
    const char *repo = std::getenv("GITHUB_REPO");
    if (token && owner && repo) {
      GitHubClient client(token);
      auto prs = client.list_pull_requests(owner, repo);
      Tui ui;
      ui.run(prs);
    } else {
      spdlog::warn("TUI skipped: GITHUB_TOKEN/OWNER/REPO not set");
    }
  }
  return 0;
}

} // namespace agpm
