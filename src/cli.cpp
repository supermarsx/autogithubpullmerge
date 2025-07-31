#include "cli.hpp"
#include <CLI/CLI.hpp>

namespace agpm {

CliOptions parse_cli(int argc, char **argv) {
  CLI::App app{"autogithubpullmerge command line"};
  CliOptions options;
  app.add_flag("-v,--verbose", options.verbose, "Enable verbose output");
  app.add_option("--config", options.config_file, "Path to configuration file")
      ->type_name("FILE");
  app.add_option(
         "--log-level", options.log_level,
         "Set logging level (trace, debug, info, warn, error, critical, off)")
      ->type_name("LEVEL")
      ->default_val("info");
  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    exit(app.exit(e));
  }
  return options;
}

} // namespace agpm
