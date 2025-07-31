#include "cli.hpp"
#include <CLI/CLI.hpp>

namespace agpm {

CliOptions parse_cli(int argc, char **argv) {
  CLI::App app{"autogithubpullmerge command line"};
  CliOptions options;
  app.add_flag("-v,--verbose", options.verbose, "Enable verbose output");
  app.add_option(
      "--log-level", options.log_level,
      "Set log level (trace, debug, info, warning, error, critical, off)");
  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    exit(app.exit(e));
  }
  return options;
}

} // namespace agpm
