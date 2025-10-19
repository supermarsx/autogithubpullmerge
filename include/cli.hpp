#ifndef AUTOGITHUBPULLMERGE_CLI_HPP
#define AUTOGITHUBPULLMERGE_CLI_HPP

#include "repo_discovery.hpp"
#include <chrono>
#include <exception>
#include <string>
#include <vector>

namespace agpm {

/**
 * Signals that CLI parsing requested an immediate exit (help, errors, etc.).
 * Used to bubble exit codes from parsing back to the main entry point without
 * treating them as fatal errors.
 */
class CliParseExit : public std::exception {
public:
  /**
   * Construct an exit signal with the desired exit code.
   *
   * @param exit_code Process exit code that should be returned to the caller.
   */
  explicit CliParseExit(int exit_code) noexcept : exit_code_(exit_code) {}

  /**
   * Retrieve the exit code that triggered the exception.
   *
   * @return Numeric process exit code.
   */
  int exit_code() const noexcept { return exit_code_; }

  /**
   * Provide an explanatory message for logging or debugging.
   *
   * @return Null-terminated string describing the exit condition.
   */
  const char *what() const noexcept override {
    return "CLI parsing requested exit";
  }

private:
  int exit_code_;
};

/**
 * Parsed command line options supplied via the CLI.
 *
 * The structure mirrors the supported CLI flags and stores post-processed
 * values so downstream components can operate without re-parsing.
 */
struct CliOptions {
  bool verbose = false;           ///< Enables verbose output
  std::string config_file;        ///< Optional path to configuration file
  std::string log_level = "info"; ///< Logging verbosity level
  std::string log_file;           ///< Optional path to rotating log file
  int log_limit{200};             ///< Maximum number of log messages to retain
  int log_rotate{3};              ///< Number of rotated log files to keep (0 disables)
  bool log_compress{false};       ///< Compress rotated log files
  bool log_rotate_explicit{false};   ///< True if CLI set log rotation count
  bool log_compress_explicit{false}; ///< True if CLI toggled log compression
  bool assume_yes{false};         ///< Skip confirmation prompts
  bool dry_run{false};            ///< Simulate operations without changes
  std::vector<std::string> include_repos; ///< Repositories to include
  std::vector<std::string> exclude_repos; ///< Repositories to exclude
  std::vector<std::string>
      protected_branches; ///< Protected branch patterns to skip
  std::vector<std::string>
      protected_branch_excludes;         ///< Patterns that remove protection
  bool include_merged{false};            ///< Include merged pull requests
  std::vector<std::string>
      repo_discovery_roots; ///< Roots to scan for local repositories
  RepoDiscoveryMode repo_discovery_mode{RepoDiscoveryMode::Disabled}; ///< Repo discovery behaviour
  bool repo_discovery_explicit{false};   ///< True if CLI set repo discovery mode
  std::vector<std::string> api_keys;     ///< Personal access tokens
  bool api_key_from_stream = false;      ///< Read tokens from stdin
  std::string api_key_url;               ///< Remote URL with tokens
  std::string api_key_url_user;          ///< Basic auth user
  std::string api_key_url_password;      ///< Basic auth password
  std::vector<std::string> api_key_files; ///< Files containing tokens
  bool auto_detect_token_files{false};   ///< Search for token files automatically
  std::vector<std::string>
      auto_detected_api_key_files; ///< Token files found automatically
  bool open_pat_window{false};           ///< Launch PAT creation page then exit
  std::string pat_save_path;             ///< Destination file for saving PAT
  std::string pat_value;                 ///< PAT value supplied via CLI
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
  std::string http_proxy;                ///< Proxy URL for HTTP requests
  std::string https_proxy;               ///< Proxy URL for HTTPS requests
  bool only_poll_prs = false;            ///< Only poll pull requests
  bool only_poll_stray = false;          ///< Only poll stray branches
  bool reject_dirty = false;             ///< Auto close dirty branches
  bool delete_stray{false};            ///< Delete stray branches automatically
  bool allow_delete_base_branch{false}; ///< Permit deleting base branches (dangerous)
  bool auto_merge{false};              ///< Automatically merge pull requests
  int required_approvals{0};           ///< Required approvals before merge
  bool require_status_success{false};  ///< Require status checks to succeed
  bool require_mergeable_state{false}; ///< Require PR to be mergeable
  std::string purge_prefix;            ///< Delete branches with this prefix
  bool purge_only = false;             ///< Only purge branches, skip PR polling
  int pr_limit{50};                    ///< Number of pull requests to fetch
  std::chrono::seconds pr_since{
      0};                  ///< Only list pull requests newer than this duration
  std::string sort;        ///< Sorting mode for pull requests
  bool use_graphql{false}; ///< Use GraphQL API for pull requests
  bool hotkeys_enabled{true};       ///< Whether interactive hotkeys are enabled
  bool hotkeys_explicit{false};     ///< True if CLI explicitly toggled hotkeys

  bool demo_tui{false};             ///< Launch mock TUI demo mode

  // Testing utilities
  std::string single_open_prs_repo; ///< OWNER/REPO for single open-PR poll
  std::string single_branches_repo; ///< OWNER/REPO for single-branch poll
};

/**
 * Parse command line arguments and return the normalized options structure.
 *
 * @param argc Number of elements supplied in @p argv.
 * @param argv Null-terminated array of raw CLI argument strings.
 * @return Populated options structure describing the requested behaviour.
 * @throws CliParseExit When parsing encounters non-error conditions such as
 *         `--help` that require the application to exit early.
 * @throws std::runtime_error On parse errors or when destructive operations
 *         are cancelled by the user.
 */
CliOptions parse_cli(int argc, char **argv);

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_CLI_HPP
