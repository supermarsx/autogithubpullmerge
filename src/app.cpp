#include "app.hpp"
#include "cli.hpp"
#include "config.hpp"
#include "log.hpp"
#include <cstdlib>
#include <iostream>
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

  // Application logic goes here
  return 0;
}

} // namespace agpm
