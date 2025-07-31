#ifndef AUTOGITHUBPULLMERGE_CLI_HPP
#define AUTOGITHUBPULLMERGE_CLI_HPP

#include <string>
#include <vector>

namespace agpm {

/** Parsed command line options. */
struct CliOptions {
  bool verbose = false;           ///< Enables verbose output
  std::string config_file;        ///< Optional path to configuration file
  std::string log_level = "info"; ///< Logging verbosity level
  std::vector<std::string> include_repos; ///< Repositories to include
  std::vector<std::string> exclude_repos; ///< Repositories to exclude
};

/**
 * Parse command line arguments.
 *
 * @param argc Number of arguments
 * @param argv Argument values
 * @return Parsed options structure
 */
CliOptions parse_cli(int argc, char **argv);

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_CLI_HPP
