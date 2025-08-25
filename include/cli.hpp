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
  std::string log_file;           ///< Optional path to rotating log file
  bool assume_yes{false};         ///< Skip confirmation prompts
  bool dry_run{false};            ///< Simulate operations without changes
  std::vector<std::string> include_repos; ///< Repositories to include
  std::vector<std::string> exclude_repos; ///< Repositories to exclude
  std::vector<std::string>
      protected_branches; ///< Protected branch patterns to skip
  std::vector<std::string>
      protected_branch_excludes;         ///< Patterns that remove protection
  bool include_merged{false};            ///< Include merged pull requests
  std::vector<std::string> api_keys;     ///< Personal access tokens
  bool api_key_from_stream = false;      ///< Read tokens from stdin
  std::string api_key_url;               ///< Remote URL with tokens
  std::string api_key_url_user;          ///< Basic auth user
  std::string api_key_url_password;      ///< Basic auth password
  std::string api_key_file;              ///< File containing tokens
  std::string history_db = "history.db"; ///< SQLite history database path
  std::string api_base;                  ///< Base URL for GitHub API
  std::string export_csv;                ///< Path to export CSV file
  std::string export_json;               ///< Path to export JSON file
  int poll_interval = 0;                 ///< Polling interval in seconds
  int max_request_rate = 60;             ///< Max requests per minute
  int workers = 0;                       ///< Number of worker threads
  int http_timeout = 30;                 ///< HTTP timeout in seconds
  int http_retries = 3;                  ///< Number of HTTP retries
  long long download_limit = 0;          ///< Download rate limit (bytes/sec)
  long long upload_limit = 0;            ///< Upload rate limit (bytes/sec)
  long long max_download = 0;            ///< Max cumulative download bytes
  long long max_upload = 0;              ///< Max cumulative upload bytes
  bool only_poll_prs = false;            ///< Only poll pull requests
  bool only_poll_stray = false;          ///< Only poll stray branches
  bool reject_dirty = false;             ///< Auto close dirty branches
  bool auto_merge{false};                ///< Automatically merge pull requests
  int required_approvals{0};             ///< Required approvals before merge
  bool require_status_success{false};    ///< Require status checks to succeed
  bool require_mergeable_state{false};   ///< Require PR to be mergeable
  std::string purge_prefix;              ///< Delete branches with this prefix
  bool purge_only = false; ///< Only purge branches, skip PR polling
  int pr_limit{50};        ///< Number of pull requests to fetch
  std::chrono::seconds pr_since{
      0};           ///< Only list pull requests newer than this duration
  std::string sort; ///< Sorting mode for pull requests
};

/**
 * Parse command line arguments.
 *
 * @param argc Number of arguments
 * @param argv Argument values
 * @return Parsed options structure
 * @throws std::runtime_error on parse errors or when destructive
 *         operations are cancelled by the user
 */
CliOptions parse_cli(int argc, char **argv);

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_CLI_HPP
