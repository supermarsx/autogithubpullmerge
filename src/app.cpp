#include "app.hpp"
#include "cli.hpp"
#include "config.hpp"
#include <iostream>

namespace agpm {

int App::run(int argc, char **argv) {
  options_ = parse_cli(argc, argv);
  if (!options_.config_file.empty()) {
    config_ = Config::from_file(options_.config_file);
  }
  if (options_.verbose) {
    std::cout << "Verbose mode enabled" << std::endl;
  }
  std::cout << "Running agpm app" << std::endl;
  return 0;
}

} // namespace agpm
