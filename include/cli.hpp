#ifndef AUTOGITHUBPULLMERGE_CLI_HPP
#define AUTOGITHUBPULLMERGE_CLI_HPP

#include <chrono>
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
  bool include_merged = false;            ///< Include merged pull requests
  std::vector<std::string> api_keys;      ///< Personal access tokens
  bool api_key_from_stream = false;       ///< Read tokens from stdin
  std::string api_key_url;                ///< Remote URL with tokens
  std::string api_key_url_user;           ///< Basic auth user
  std::string api_key_url_password;       ///< Basic auth password
  std::string api_key_file;               ///< File containing tokens
  std::string history_db = "history.db";  ///< SQLite history database path
  int poll_interval = 0;                  ///< Polling interval in seconds
  int max_request_rate = 60;              ///< Max requests per minute
  bool poll_prs_only = false;             ///< Only poll pull requests
  bool poll_stray_only = false;           ///< Only poll stray branches
  int pr_limit{50};                       ///< Number of pull requests to fetch
  std::chrono::seconds pr_since{0}; ///< Only list pull requests newer than this
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
