#include "app.hpp"
#include "cli.hpp"
#include "log.hpp"

namespace agpm {

int App::run(int argc, char **argv) {
  options_ = parse_cli(argc, argv);
  init_logger(level_from_string(options_.log_level), options_.log_file);
  if (options_.verbose) {
    logger()->info("Verbose mode enabled");
  }
  logger()->info("Running agpm app");
  return 0;
}

} // namespace agpm
