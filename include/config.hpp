#ifndef AUTOGITHUBPULLMERGE_CONFIG_HPP
#define AUTOGITHUBPULLMERGE_CONFIG_HPP

#include "repo_discovery.hpp"
#include <chrono>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace agpm {

/// Application configuration loaded from a YAML, TOML, or JSON file.
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

  /// Maximum requests per hour (0 = auto-detected).
  int max_hourly_requests() const { return max_hourly_requests_; }

  /// Set maximum hourly request limit (0 = auto).
  void set_max_hourly_requests(int requests) {
    max_hourly_requests_ = requests < 0 ? 0 : requests;
  }

  /// Number of worker threads used for polling operations.
  int workers() const { return workers_; }

  /// Set worker thread count (minimum 1).
  void set_workers(int w) { workers_ = w < 1 ? 1 : w; }

  /// HTTP request timeout in seconds.
  int http_timeout() const { return http_timeout_; }

  /// Set HTTP request timeout.
  void set_http_timeout(int t) { http_timeout_ = t; }

  /// Number of HTTP retry attempts.
  int http_retries() const { return http_retries_; }

  /// Set number of HTTP retry attempts.
  void set_http_retries(int r) { http_retries_ = r; }

  /// Base URL for the GitHub API.
  const std::string &api_base() const { return api_base_; }

  /// Set base URL for the GitHub API.
  void set_api_base(const std::string &base) { api_base_ = base; }

  /// Download rate limit in bytes per second (0 = unlimited).
  long long download_limit() const { return download_limit_; }

  /// Set download rate limit.
  void set_download_limit(long long limit) { download_limit_ = limit; }

  /// Upload rate limit in bytes per second (0 = unlimited).
  long long upload_limit() const { return upload_limit_; }

  /// Set upload rate limit.
  void set_upload_limit(long long limit) { upload_limit_ = limit; }

  /// Maximum cumulative download in bytes (0 = unlimited).
  long long max_download() const { return max_download_; }

  /// Set maximum cumulative download.
  void set_max_download(long long bytes) { max_download_ = bytes; }

  /// Maximum cumulative upload in bytes (0 = unlimited).
  long long max_upload() const { return max_upload_; }

  /// Set maximum cumulative upload.
  void set_max_upload(long long bytes) { max_upload_ = bytes; }

  /// Proxy URL for HTTP requests.
  const std::string &http_proxy() const { return http_proxy_; }

  /// Set proxy URL for HTTP requests.
  void set_http_proxy(const std::string &proxy) { http_proxy_ = proxy; }

  /// Proxy URL for HTTPS requests.
  const std::string &https_proxy() const { return https_proxy_; }

  /// Set proxy URL for HTTPS requests.
  void set_https_proxy(const std::string &proxy) { https_proxy_ = proxy; }

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

  /// Maximum number of in-memory log messages to keep.
  int log_limit() const { return log_limit_; }

  /// Set maximum number of in-memory log messages.
  void set_log_limit(int limit) { log_limit_ = limit; }

  /// Number of rotated log files to retain (0 disables rotation).
  int log_rotate() const { return log_rotate_; }

  /// Set number of rotated log files to retain.
  void set_log_rotate(int count) { log_rotate_ = count < 0 ? 0 : count; }

  /// Whether rotated log files are compressed.
  bool log_compress() const { return log_compress_; }

  /// Enable or disable compression of rotated log files.
  void set_log_compress(bool enable) { log_compress_ = enable; }

  /// Return whether the logger sidecar window is enabled.
  bool log_sidecar() const { return log_sidecar_; }

  /// Enable or disable the logger sidecar window.
  void set_log_sidecar(bool enable) { log_sidecar_ = enable; }

  /// Retrieve configured log category overrides.
  const std::unordered_map<std::string, std::string> &log_categories() const {
    return log_categories_;
  }

  /// Replace configured log category overrides.
  void set_log_categories(std::unordered_map<std::string, std::string> values) {
    log_categories_ = std::move(values);
  }

  /// Set or update a single log category override.
  void set_log_category(const std::string &name, const std::string &level) {
    log_categories_[name] = level;
  }

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

  /// Branch patterns to protect from deletion.
  const std::vector<std::string> &protected_branches() const {
    return protected_branches_;
  }

  /// Set protected branch patterns.
  void set_protected_branches(const std::vector<std::string> &branches) {
    protected_branches_ = branches;
  }

  /// Branch patterns to explicitly unprotect.
  const std::vector<std::string> &protected_branch_excludes() const {
    return protected_branch_excludes_;
  }

  /// Set branch patterns to explicitly unprotect.
  void set_protected_branch_excludes(const std::vector<std::string> &branches) {
    protected_branch_excludes_ = branches;
  }

  /// Whether to include merged pull requests.
  bool include_merged() const { return include_merged_; }

  /// Set inclusion of merged pull requests.
  void set_include_merged(bool include) { include_merged_ = include; }

  /// Repository discovery mode.
  RepoDiscoveryMode repo_discovery_mode() const { return repo_discovery_mode_; }

  /// Set repository discovery mode.
  void set_repo_discovery_mode(RepoDiscoveryMode mode) {
    repo_discovery_mode_ = mode;
  }

  /// Paths scanned for filesystem repository discovery.
  const std::vector<std::string> &repo_discovery_roots() const {
    return repo_discovery_roots_;
  }

  /// Set paths for filesystem repository discovery.
  void set_repo_discovery_roots(const std::vector<std::string> &roots) {
    repo_discovery_roots_ = roots;
  }

  /// Append a single filesystem discovery root.
  void add_repo_discovery_root(const std::string &root) {
    repo_discovery_roots_.push_back(root);
  }

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

  /// Paths to files containing API keys.
  const std::vector<std::string> &api_key_files() const {
    return api_key_files_;
  }

  /// Set paths to files containing API keys.
  void set_api_key_files(const std::vector<std::string> &paths) {
    api_key_files_ = paths;
  }

  /// Append a single API key file path.
  void add_api_key_file(const std::string &path) {
    api_key_files_.push_back(path);
  }

  /// Path to SQLite history database.
  const std::string &history_db() const { return history_db_; }

  /// Set history database path.
  void set_history_db(const std::string &path) { history_db_ = path; }

  /// CSV export destination.
  const std::string &export_csv() const { return export_csv_; }

  /// Set CSV export destination.
  void set_export_csv(const std::string &path) { export_csv_ = path; }

  /// JSON export destination.
  const std::string &export_json() const { return export_json_; }

  /// Set JSON export destination.
  void set_export_json(const std::string &path) { export_json_ = path; }

  /// Automatically answer yes to destructive confirmations.
  bool assume_yes() const { return assume_yes_; }

  /// Set automatic confirmation behaviour.
  void set_assume_yes(bool yes) { assume_yes_ = yes; }

  /// Run in dry-run mode.
  bool dry_run() const { return dry_run_; }

  /// Set dry-run behaviour.
  void set_dry_run(bool v) { dry_run_ = v; }

  /// Only poll pull requests.
  bool only_poll_prs() const { return only_poll_prs_; }

  /// Set only poll pull requests flag.
  void set_only_poll_prs(bool v) { only_poll_prs_ = v; }

  /// Only poll stray branches.
  bool only_poll_stray() const { return only_poll_stray_; }

  /// Set only poll stray flag.
  void set_only_poll_stray(bool v) { only_poll_stray_ = v; }

  /// Only purge stray branches without polling PRs.
  bool purge_only() const { return purge_only_; }

  /// Set purge only flag.
  void set_purge_only(bool v) { purge_only_ = v; }

  /// Auto reject dirty branches.
  bool reject_dirty() const { return reject_dirty_; }

  /// Set reject dirty flag.
  void set_reject_dirty(bool v) { reject_dirty_ = v; }

  /// Automatically merge pull requests.
  bool auto_merge() const { return auto_merge_; }

  /// Set auto merge flag.
  void set_auto_merge(bool v) { auto_merge_ = v; }

  /// Merge rule configuration
  /// Required number of approvals before merging.
  int required_approvals() const { return required_approvals_; }

  /// Set required approvals.
  void set_required_approvals(int n) { required_approvals_ = n; }

  /// Require successful status checks before merging.
  bool require_status_success() const { return require_status_success_; }

  /// Set require status checks flag.
  void set_require_status_success(bool v) { require_status_success_ = v; }

  /// Require pull request to be mergeable.
  bool require_mergeable_state() const { return require_mergeable_state_; }

  /// Set require mergeable state flag.
  void set_require_mergeable_state(bool v) { require_mergeable_state_ = v; }

  /// Prefix of branches to purge after merge.
  const std::string &purge_prefix() const { return purge_prefix_; }

  /// Set purge prefix for branch deletion.
  void set_purge_prefix(const std::string &p) { purge_prefix_ = p; }

  /// Delete stray branches automatically.
  bool delete_stray() const { return delete_stray_; }

  /// Set delete stray flag.
  void set_delete_stray(bool v) { delete_stray_ = v; }

  /// Use heuristics to detect stray branches.
  bool heuristic_stray_detection() const { return heuristic_stray_detection_; }

  /// Set heuristic stray detection flag.
  void set_heuristic_stray_detection(bool v) { heuristic_stray_detection_ = v; }

  /// Allow deleting base branches.
  bool allow_delete_base_branch() const { return allow_delete_base_branch_; }

  /// Set allow delete base branch flag.
  void set_allow_delete_base_branch(bool v) { allow_delete_base_branch_ = v; }

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

  /// Use the GraphQL API for pull request queries.
  bool use_graphql() const { return use_graphql_; }

  /// Enable or disable GraphQL usage.
  void set_use_graphql(bool v) { use_graphql_ = v; }

  /// Fraction of the hourly GitHub rate limit kept in reserve.
  double rate_limit_margin() const { return rate_limit_margin_; }

  /// Set the fraction of the hourly GitHub rate limit kept in reserve.
  void set_rate_limit_margin(double margin);

  /// Interval between rate limit endpoint checks in seconds.
  int rate_limit_refresh_interval() const {
    return rate_limit_refresh_interval_;
  }

  /// Set the interval between rate limit endpoint checks (seconds).
  void set_rate_limit_refresh_interval(int seconds) {
    rate_limit_refresh_interval_ = seconds <= 0 ? 60 : seconds;
  }

  /// Whether to continue querying the rate limit endpoint after failures.
  bool retry_rate_limit_endpoint() const { return retry_rate_limit_endpoint_; }

  /// Enable or disable retrying the rate limit endpoint after failures.
  void set_retry_rate_limit_endpoint(bool enable) {
    retry_rate_limit_endpoint_ = enable;
  }

  /// Maximum scheduled retries of the rate limit endpoint when retries enabled.
  int rate_limit_retry_limit() const { return rate_limit_retry_limit_; }

  /// Set maximum scheduled retries of the rate limit endpoint when enabled.
  void set_rate_limit_retry_limit(int limit) {
    rate_limit_retry_limit_ = limit <= 0 ? 1 : limit;
  }

  /// Determine whether TUI hotkeys are enabled.
  bool hotkeys_enabled() const { return hotkeys_enabled_; }

  /// Set hotkey enablement.
  void set_hotkeys_enabled(bool enabled) { hotkeys_enabled_ = enabled; }

  /// Retrieve custom hotkey bindings (action -> key spec).
  const std::unordered_map<std::string, std::string> &hotkey_bindings() const {
    return hotkey_bindings_;
  }

  /// Replace hotkey bindings.
  void
  set_hotkey_bindings(std::unordered_map<std::string, std::string> values) {
    hotkey_bindings_ = std::move(values);
  }

  /// Assign or update a single hotkey binding.
  void set_hotkey_binding(const std::string &action, const std::string &key) {
    hotkey_bindings_[action] = key;
  }

  /// Perform a single open-PR fetch for testing purposes.
  const std::string &single_open_prs_repo() const {
    return single_open_prs_repo_;
  }

  /// Set repository for single open-PR fetch.
  void set_single_open_prs_repo(const std::string &repo) {
    single_open_prs_repo_ = repo;
  }

  /// Perform a single branch list fetch for testing purposes.
  const std::string &single_branches_repo() const {
    return single_branches_repo_;
  }

  /// Set repository for single branch list fetch.
  void set_single_branches_repo(const std::string &repo) {
    single_branches_repo_ = repo;
  }

  /// Should the PAT creation page open automatically.
  bool open_pat_page() const { return open_pat_page_; }

  /// Set automatic PAT page launch flag.
  void set_open_pat_page(bool v) { open_pat_page_ = v; }

  /// Destination file to save a PAT.
  const std::string &pat_save_path() const { return pat_save_path_; }

  /// Set destination file for saving a PAT.
  void set_pat_save_path(const std::string &path) { pat_save_path_ = path; }

  /// PAT value provided in configuration.
  const std::string &pat_value() const { return pat_value_; }

  /// Set PAT value provided by configuration.
  void set_pat_value(const std::string &value) { pat_value_ = value; }

  /// Load configuration from the file at `path`.
  static Config from_file(const std::string &path);

  /// Build configuration from a JSON object.
  static Config from_json(const nlohmann::json &j);

  /// Populate this configuration from a JSON object.
  void load_json(const nlohmann::json &j);

private:
  bool verbose_ = false;
  int poll_interval_ = 0;
  int max_request_rate_ = 60;
  int max_hourly_requests_ = 0;
  int workers_ = 4; ///< Default number of worker threads
  std::string log_level_ = "info";
  std::string log_pattern_;
  std::string log_file_;
  int log_limit_ = 200;
  int log_rotate_ = 3;
  bool log_compress_ = false;
  bool log_sidecar_ = false;
  std::unordered_map<std::string, std::string> log_categories_;
  std::vector<std::string> include_repos_;
  std::vector<std::string> exclude_repos_;
  std::vector<std::string> protected_branches_;
  std::vector<std::string> protected_branch_excludes_;
  bool include_merged_ = false;
  RepoDiscoveryMode repo_discovery_mode_ = RepoDiscoveryMode::Disabled;
  std::vector<std::string> repo_discovery_roots_;
  std::vector<std::string> api_keys_;
  bool api_key_from_stream_ = false;
  std::string api_key_url_;
  std::string api_key_url_user_;
  std::string api_key_url_password_;
  std::vector<std::string> api_key_files_;
  std::string history_db_ = "history.db";
  std::string export_csv_;
  std::string export_json_;
  bool assume_yes_ = false;
  bool dry_run_ = false;
  bool only_poll_prs_ = false;
  bool only_poll_stray_ = false;
  bool purge_only_ = false;
  bool reject_dirty_ = false;
  bool auto_merge_ = false;
  int required_approvals_ = 0;
  bool require_status_success_ = false;
  bool require_mergeable_state_ = false;
  std::string purge_prefix_;
  int pr_limit_ = 50;
  std::chrono::seconds pr_since_{0};
  std::string sort_mode_;
  bool use_graphql_ = false;
  bool hotkeys_enabled_ = true;
  std::unordered_map<std::string, std::string> hotkey_bindings_;
  int http_timeout_ = 30;
  int http_retries_ = 3;
  std::string api_base_ = "https://api.github.com";
  double rate_limit_margin_ = 0.7;
  int rate_limit_refresh_interval_ = 60;
  bool retry_rate_limit_endpoint_ = false;
  int rate_limit_retry_limit_ = 3;
  long long download_limit_ = 0;
  long long upload_limit_ = 0;
  long long max_download_ = 0;
  long long max_upload_ = 0;
  std::string http_proxy_;
  std::string https_proxy_;
  bool delete_stray_ = false;
  bool heuristic_stray_detection_ = false;
  bool allow_delete_base_branch_ = false;
  bool open_pat_page_ = false;
  std::string pat_save_path_;
  std::string pat_value_;
  std::string single_open_prs_repo_;
  std::string single_branches_repo_;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_CONFIG_HPP
