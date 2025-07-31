
#include "cli.hpp"
#include <string>

namespace agpm {

CliOptions parse_cli(int argc, char **argv) {
  CliOptions options;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-v" || arg == "--verbose") {
      options.verbose = true;
    } else if (arg == "--config" && i + 1 < argc) {
      options.config_file = argv[++i];
    } else if (arg == "--log-level" && i + 1 < argc) {
      options.log_level = argv[++i];
    } else if (arg == "--include" && i + 1 < argc) {
      options.include_repos.push_back(argv[++i]);
    } else if (arg == "--exclude" && i + 1 < argc) {
      options.exclude_repos.push_back(argv[++i]);
    }
  }
  return options;
}

} // namespace agpm
