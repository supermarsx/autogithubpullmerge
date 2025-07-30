#include "app.hpp"
#include "cli.hpp"
#include "tui.hpp"
#include <iostream>

namespace agpm {

int App::run(int argc, char **argv) {
  options_ = parse_cli(argc, argv);
  if (options_.verbose) {
    std::cout << "Verbose mode enabled" << std::endl;
  }
  std::cout << "Running agpm app" << std::endl;
  render_pr_list();
  return 0;
}

} // namespace agpm
