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
  std::vector<std::string> api_keys;      ///< Personal access tokens
  bool api_key_from_stream = false;       ///< Read tokens from stdin
  std::string api_key_url;                ///< Remote URL with tokens
  std::string api_key_url_user;           ///< Basic auth user
  std::string api_key_url_password;       ///< Basic auth password
  std::string api_key_file;               ///< File containing tokens
  int poll_interval = 0;                  ///< Polling interval in seconds
  int max_request_rate = 60;              ///< Max requests per minute
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
