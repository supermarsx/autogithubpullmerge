#ifndef AUTOGITHUBPULLMERGE_CONFIG_HPP
#define AUTOGITHUBPULLMERGE_CONFIG_HPP

#include <chrono>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

namespace agpm {

/// Application configuration loaded from a YAML or JSON file.
class Config {
public:
  /** Check whether verbose output is enabled. */
  bool verbose() const { return verbose_; }

  /// Set verbose output mode.
  void set_verbose(bool verbose) { verbose_ = verbose; }

  /// Polling interval in seconds.
  int poll_interval() const { return poll_interval_; }

  /// Set polling interval.
  void set_poll_interval(int interval) { poll_interval_ = interval; }

  /// Maximum requests per minute.
  int max_request_rate() const { return max_request_rate_; }

  /// Set maximum request rate.
  void set_max_request_rate(int rate) { max_request_rate_ = rate; }

  /// HTTP request timeout in seconds.
  int http_timeout() const { return http_timeout_; }

  /// Set HTTP request timeout.
  void set_http_timeout(int t) { http_timeout_ = t; }

  /// Number of HTTP retry attempts.
  int http_retries() const { return http_retries_; }

  /// Set number of HTTP retry attempts.
  void set_http_retries(int r) { http_retries_ = r; }

  /// Get logging verbosity level.
  const std::string &log_level() const { return log_level_; }

  /// Set logging verbosity level.
  void set_log_level(const std::string &level) { log_level_ = level; }

  /// Get logging pattern.
  const std::string &log_pattern() const { return log_pattern_; }

  /// Set logging pattern.
  void set_log_pattern(const std::string &pattern) { log_pattern_ = pattern; }

  /// Path to rotating log file.
  const std::string &log_file() const { return log_file_; }

  /// Set path for rotating log file.
  void set_log_file(const std::string &file) { log_file_ = file; }

  /// Repositories to include.
  const std::vector<std::string> &include_repos() const {
    return include_repos_;
  }

  /// Set repositories to include.
  void set_include_repos(const std::vector<std::string> &repos) {
    include_repos_ = repos;
  }

  /// Repositories to exclude.
  const std::vector<std::string> &exclude_repos() const {
    return exclude_repos_;
  }

  /// Set repositories to exclude.
  void set_exclude_repos(const std::vector<std::string> &repos) {
    exclude_repos_ = repos;
  }

  /// Whether to include merged pull requests.
  bool include_merged() const { return include_merged_; }

  /// Set inclusion of merged pull requests.
  void set_include_merged(bool include) { include_merged_ = include; }

  /// Configured API keys.
  const std::vector<std::string> &api_keys() const { return api_keys_; }

  /// Set API keys.
  void set_api_keys(const std::vector<std::string> &keys) { api_keys_ = keys; }

  /// Read API keys from stdin.
  bool api_key_from_stream() const { return api_key_from_stream_; }

  /// Enable or disable reading API keys from stdin.
  void set_api_key_from_stream(bool from_stream) {
    api_key_from_stream_ = from_stream;
  }

  /// URL to fetch API keys from.
  const std::string &api_key_url() const { return api_key_url_; }

  /// Set URL to fetch API keys from.
  void set_api_key_url(const std::string &url) { api_key_url_ = url; }

  /// Username for API key URL basic auth.
  const std::string &api_key_url_user() const { return api_key_url_user_; }

  /// Set username for API key URL basic auth.
  void set_api_key_url_user(const std::string &user) {
    api_key_url_user_ = user;
  }

  /// Password for API key URL basic auth.
  const std::string &api_key_url_password() const {
    return api_key_url_password_;
  }

  /// Set password for API key URL basic auth.
  void set_api_key_url_password(const std::string &pass) {
    api_key_url_password_ = pass;
  }

  /// Path to file containing API keys.
  const std::string &api_key_file() const { return api_key_file_; }

  /// Set path to file containing API keys.
  void set_api_key_file(const std::string &path) { api_key_file_ = path; }

  /// Path to SQLite history database.
  const std::string &history_db() const { return history_db_; }

  /// Set history database path.
  void set_history_db(const std::string &path) { history_db_ = path; }

  /// Only poll pull requests.
  bool only_poll_prs() const { return only_poll_prs_; }

  /// Set only poll pull requests flag.
  void set_only_poll_prs(bool v) { only_poll_prs_ = v; }

  /// Only poll stray branches.
  bool only_poll_stray() const { return only_poll_stray_; }

  /// Set only poll stray flag.
  void set_only_poll_stray(bool v) { only_poll_stray_ = v; }

  /// Auto reject dirty branches.
  bool reject_dirty() const { return reject_dirty_; }

  /// Set reject dirty flag.
  void set_reject_dirty(bool v) { reject_dirty_ = v; }

  /// Automatically merge pull requests.
  bool auto_merge() const { return auto_merge_; }

  /// Set auto merge flag.
  void set_auto_merge(bool v) { auto_merge_ = v; }

  /// Prefix of branches to purge after merge.
  const std::string &purge_prefix() const { return purge_prefix_; }

  /// Set purge prefix for branch deletion.
  void set_purge_prefix(const std::string &p) { purge_prefix_ = p; }

  /// Limit of pull requests to fetch.
  int pr_limit() const { return pr_limit_; }

  /// Set limit of pull requests to fetch.
  void set_pr_limit(int limit) { pr_limit_ = limit; }

  /// Only list pull requests newer than this duration.
  std::chrono::seconds pr_since() const { return pr_since_; }

  /// Set duration for filtering pull requests.
  void set_pr_since(std::chrono::seconds since) { pr_since_ = since; }

  /// Sorting mode for pull request listing.
  const std::string &sort_mode() const { return sort_mode_; }

  /// Set sorting mode for pull request listing.
  void set_sort_mode(const std::string &mode) { sort_mode_ = mode; }

  /// Load configuration from the file at `path`.
  static Config from_file(const std::string &path);

  /// Build configuration from a JSON object.
  static Config from_json(const nlohmann::json &j);

private:
  bool verbose_ = false;
  int poll_interval_ = 0;
  int max_request_rate_ = 60;
  std::string log_level_ = "info";
  std::string log_pattern_;
  std::string log_file_;
  std::vector<std::string> include_repos_;
  std::vector<std::string> exclude_repos_;
  bool include_merged_ = false;
  std::vector<std::string> api_keys_;
  bool api_key_from_stream_ = false;
  std::string api_key_url_;
  std::string api_key_url_user_;
  std::string api_key_url_password_;
  std::string api_key_file_;
  std::string history_db_ = "history.db";
  bool only_poll_prs_ = false;
  bool only_poll_stray_ = false;
  bool reject_dirty_ = false;
  bool auto_merge_ = false;
  std::string purge_prefix_;
  int pr_limit_ = 50;
  std::chrono::seconds pr_since_{0};
  std::string sort_mode_;
  int http_timeout_ = 30;
  int http_retries_ = 3;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_CONFIG_HPP
