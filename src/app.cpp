#include "app.hpp"
#include "cli.hpp"
#include "log.hpp"
#include <iostream>
#include <spdlog/spdlog.h>

namespace agpm {

int App::run(int argc, char **argv) {
  options_ = parse_cli(argc, argv);
  spdlog::level::level_enum lvl = spdlog::level::from_str(options_.log_level);
  init_logging(lvl);
  if (options_.verbose) {
    std::cout << "Verbose mode enabled" << std::endl;
  }
  std::cout << "Running agpm app" << std::endl;
  return 0;
}

} // namespace agpm
