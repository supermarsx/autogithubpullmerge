#ifndef AUTOGITHUBPULLMERGE_GITHUB_CLIENT_HPP
#define AUTOGITHUBPULLMERGE_GITHUB_CLIENT_HPP

#include <chrono>
#include <curl/curl.h>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace agpm {

/**
 * Simple HTTP response container capturing body, headers, and status code.
 */
struct HttpResponse {
  std::string body;                 ///< Response body
  std::vector<std::string> headers; ///< Response headers
  long status_code = 0;             ///< HTTP status code
};

/** Interface for performing HTTP requests. */
class HttpClient {
public:
  virtual ~HttpClient() = default;
  /**
   * Perform a HTTP GET request.
   *
   * @param url Absolute request URL.
   * @param headers Additional request headers expressed as `Header: value`
   *        strings.
   * @return Response body content as a UTF-8 string.
   * @throws std::runtime_error On transport or protocol failures.
   */
  virtual std::string get(const std::string &url,
                          const std::vector<std::string> &headers) = 0;

  /**
   * Perform a HTTP GET request returning both body and response headers.
   *
   * @param url Absolute request URL.
   * @param headers Additional request headers expressed as `Header: value`
   *        strings.
   * @return Aggregated response body, headers, and HTTP status code.
   * @throws std::runtime_error On transport or protocol failures.
   */
  virtual HttpResponse
  get_with_headers(const std::string &url,
                   const std::vector<std::string> &headers) {
    return {get(url, headers), {}, 200};
  }

  /**
   * Perform a HTTP PUT request.
   *
   * @param url Absolute request URL.
   * @param data Request body payload encoded as UTF-8.
   * @param headers Additional request headers expressed as `Header: value`
   *        strings.
   * @return Response body content as a UTF-8 string.
   * @throws std::runtime_error On transport or protocol failures.
   */
  virtual std::string put(const std::string &url, const std::string &data,
                          const std::vector<std::string> &headers) = 0;

  /**
   * Perform a HTTP PATCH request.
   *
   * Subclasses overriding this method should issue a PATCH call with the
   * provided payload. The base implementation throws to signal unsupported
   * transports.
   */
  virtual std::string patch(const std::string &url, const std::string &data,
                            const std::vector<std::string> &headers) {
    (void)url;
    (void)data;
    (void)headers;
    throw std::runtime_error("PATCH not implemented");
  }

  /**
   * Perform a HTTP DELETE request.
   *
   * @param url Absolute request URL.
   * @param headers Additional request headers expressed as `Header: value`
   *        strings.
   * @return Response body content as a UTF-8 string.
   * @throws std::runtime_error On transport or protocol failures.
   */
  virtual std::string del(const std::string &url,
                          const std::vector<std::string> &headers) = 0;
};

/**
 * RAII wrapper for a CURL easy handle ensuring global CURL initialization.
 */
class CurlHandle {
public:
  CurlHandle();
  ~CurlHandle();
  CurlHandle(const CurlHandle &) = delete;
  CurlHandle &operator=(const CurlHandle &) = delete;
  /**
   * Access the underlying CURL easy handle.
   *
   * @return Borrowed pointer to the CURL easy handle managed by the wrapper.
   */
  CURL *get() const { return handle_; }

private:
  CURL *handle_;
};

/**
 * CURL-based HTTP client implementation.
 *
 * @note This class is not thread-safe; use one instance per thread or provide
 *       external synchronization.
 */
class CurlHttpClient : public HttpClient {
public:
  /**
   * Construct a CURL based HTTP client.
   *
   * @param timeout_ms Request timeout in milliseconds for individual
   *        operations.
   * @param download_limit Maximum download rate in bytes per second (0 =
   *        unlimited).
   * @param upload_limit Maximum upload rate in bytes per second (0 =
   *        unlimited).
   * @param max_download Maximum cumulative download in bytes before refusing
   *        further requests (0 = unlimited).
   * @param max_upload Maximum cumulative upload in bytes before refusing
   *        further requests (0 = unlimited).
   * @param http_proxy Proxy URL for HTTP requests.
   * @param https_proxy Proxy URL for HTTPS requests.
   */
  explicit CurlHttpClient(long timeout_ms = 30000,
                          curl_off_t download_limit = 0,
                          curl_off_t upload_limit = 0,
                          curl_off_t max_download = 0,
                          curl_off_t max_upload = 0,
                          std::string http_proxy = {},
                          std::string https_proxy = {});

  /// @copydoc HttpClient::get()
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override;

  /// @copydoc HttpClient::get_with_headers()
  HttpResponse
  get_with_headers(const std::string &url,
                   const std::vector<std::string> &headers) override;

  /// @copydoc HttpClient::put()
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override;

  /// @copydoc HttpClient::patch()
  std::string patch(const std::string &url, const std::string &data,
                    const std::vector<std::string> &headers) override;

  /// @copydoc HttpClient::del()
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override;

  /// Total bytes downloaded so far.
  curl_off_t total_downloaded() const { return total_downloaded_; }

  /// Total bytes uploaded so far.
  curl_off_t total_uploaded() const { return total_uploaded_; }

  /// Download limit in bytes per second.
  curl_off_t download_limit() const { return download_limit_; }

  /// Upload limit in bytes per second.
  curl_off_t upload_limit() const { return upload_limit_; }

  /// Maximum cumulative download in bytes.
  curl_off_t max_download() const { return max_download_; }

  /// Maximum cumulative upload in bytes.
  curl_off_t max_upload() const { return max_upload_; }

  /// HTTP proxy URL.
  const std::string &http_proxy() const { return http_proxy_; }

  /// HTTPS proxy URL.
  const std::string &https_proxy() const { return https_proxy_; }

private:
  void apply_proxy(CURL *curl, const std::string &url);
  CurlHandle curl_;
  long timeout_ms_;
  curl_off_t download_limit_;
  curl_off_t upload_limit_;
  curl_off_t max_download_;
  curl_off_t max_upload_;
  std::string http_proxy_;
  std::string https_proxy_;
  curl_off_t total_downloaded_{0};
  curl_off_t total_uploaded_{0};
};

/// Representation of a GitHub pull request.
struct PullRequest {
  int number;          ///< PR number
  std::string title;   ///< PR title
  bool merged{};       ///< Whether the PR has been merged
  std::string owner{}; ///< Repository owner
  std::string repo{};  ///< Repository name
};

/// Enumeration describing the CI check result for a pull request.
enum class PullRequestCheckState {
  Unknown, ///< No information about checks is available.
  Passed,  ///< All required or configured checks completed successfully.
  Rejected ///< One or more checks failed.
};

/// Lightweight metadata describing the state of a pull request.
struct PullRequestMetadata {
  int approvals{0};                 ///< Recorded approval count.
  bool mergeable{false};            ///< Mergeability flag reported by GitHub.
  std::string mergeable_state{};    ///< Detailed mergeability state string.
  std::string state{};              ///< PR state ("open", "closed", ...).
  bool draft{false};                ///< Indicates the PR is a draft.
  PullRequestCheckState check_state{///< Summary of CI check outcomes.
                                    PullRequestCheckState::Unknown};
};

/// Representation of a stray branch detected during polling.
struct StrayBranch {
  std::string owner; ///< Repository owner
  std::string repo;  ///< Repository name
  std::string name;  ///< Branch name
};

/**
 * Simple GitHub REST API client that encapsulates authentication, retries, and
 * repository filtering.
 */
class GitHubClient {
public:
  /**
   * Construct a GitHub API client.
   *
   * @param tokens Personal access tokens used for authenticated requests. The
   *        client automatically rotates through them to distribute rate limits.
   * @param http Optional HTTP client implementation. A default CURL-backed
   *        implementation is constructed when `nullptr` is supplied.
   * @param include_repos Repository identifiers (`owner/repo`) that are
   *        explicitly allowed. An empty set permits all repositories.
   * @param exclude_repos Repository identifiers that should be skipped even if
   *        otherwise permitted.
   * @param delay_ms Minimum delay between requests in milliseconds to respect
   *        rate limits.
   * @param timeout_ms HTTP request timeout in milliseconds for internally
   *        created HTTP clients.
   * @param max_retries Number of retry attempts for transient failures.
   * @param api_base Base URL for the GitHub API endpoints.
   * @param dry_run When true, destructive operations such as merges or branch
   *        deletions are simulated instead of performed.
   * @param cache_file Optional filesystem path used to persist ETag-based cache
   *        entries between runs.
   */
  explicit GitHubClient(std::vector<std::string> tokens,
                        std::unique_ptr<HttpClient> http = nullptr,
                        std::unordered_set<std::string> include_repos = {},
                        std::unordered_set<std::string> exclude_repos = {},
                        int delay_ms = 0, int timeout_ms = 30000,
                        int max_retries = 3,
                        std::string api_base = "https://api.github.com",
                        bool dry_run = false, std::string cache_file = "");

  ~GitHubClient();

  /// Set minimum delay between HTTP requests in milliseconds.
  void set_delay_ms(int delay_ms);

  /// Set required approvals before merging.
  void set_required_approvals(int n) {
    std::scoped_lock lock(mutex_);
    required_approvals_ = n;
  }

  /// Set whether successful status checks are required before merging.
  void set_require_status_success(bool v) {
    std::scoped_lock lock(mutex_);
    require_status_success_ = v;
  }

  /// Set whether a PR must be mergeable before merging.
  void set_require_mergeable_state(bool v) {
    std::scoped_lock lock(mutex_);
    require_mergeable_state_ = v;
  }

  /// Set whether base branches such as main/master may be deleted.
  void set_allow_delete_base_branch(bool v) {
    std::scoped_lock lock(mutex_);
    allow_delete_base_branch_ = v;
  }

  /**
   * List repositories accessible to the authenticated user.
   *
   * @return List of repository `owner`/`name` pairs visible to the provided
   *         credentials after filtering against include/exclude rules.
   */
  std::vector<std::pair<std::string, std::string>> list_repositories();

  /**
   * List pull requests for a repository.
   *
   * @param owner Repository owner.
   * @param repo Repository name.
   * @param include_merged Include merged pull requests when true.
   * @param per_page Number of pull requests to fetch per page (max 100).
   * @param since Only include pull requests updated on or after this timestamp.
   * @return List of pull request summaries retrieved from the REST API.
   */
  std::vector<PullRequest>
  list_pull_requests(const std::string &owner, const std::string &repo,
                     bool include_merged = false, int per_page = 50,
                     std::chrono::seconds since = std::chrono::seconds{0});

  /**
   * Perform a single HTTP request to list currently open pull requests for a
   * repository. Intended for tests that must avoid pagination and extra
   * requests.
   *
   * @param owner_repo Repository in "owner/repo" format.
   * @param per_page Maximum number of PRs to request (default 100).
   * @return List of open pull requests from a single page.
   */
  std::vector<PullRequest>
  list_open_pull_requests_single(const std::string &owner_repo,
                                 int per_page = 100);

  /**
   * Merge a pull request.
   *
   * @param owner Repository owner.
   * @param repo Repository name.
   * @param pr_number Pull request number.
   * @return `true` if the merge request was accepted by the API, otherwise
   *         `false`.
   */
  bool merge_pull_request(const std::string &owner, const std::string &repo,
                          int pr_number);

  /// Merge a pull request using previously fetched metadata.
  bool merge_pull_request(const std::string &owner, const std::string &repo,
                          int pr_number, const PullRequestMetadata &metadata);

  /// Close a pull request without merging.
  bool close_pull_request(const std::string &owner, const std::string &repo,
                          int pr_number);

  /**
   * Delete a branch ref from a repository.
   *
   * @param owner Repository owner.
   * @param repo Repository name.
   * @param branch Branch ref to delete.
   * @param protected_branches Patterns that must not be deleted.
   * @param protected_branch_excludes Patterns overriding protections.
   * @return `true` when the branch was deleted or simulated successfully.
   */
  bool
  delete_branch(const std::string &owner, const std::string &repo,
                const std::string &branch,
                const std::vector<std::string> &protected_branches = {},
                const std::vector<std::string> &protected_branch_excludes = {});

  /// Fetch metadata describing a pull request's current state.
  std::optional<PullRequestMetadata>
  pull_request_metadata(const std::string &owner, const std::string &repo,
                        int pr_number);

  /**
   * List branch names for a repository excluding the default branch.
   *
   * @param owner Repository owner.
   * @param repo Repository name.
   * @return All non-default branch names that survive include/exclude filters.
   */
  std::vector<std::string> list_branches(const std::string &owner,
                                         const std::string &repo,
                                         std::string *default_branch = nullptr);

  /**
   * Identify branches that appear stray based on heuristic signals.
   *
   * @param owner Repository owner.
   * @param repo Repository name.
   * @param default_branch Resolved default branch used as the comparison base.
   * @param branches Candidate branch names to analyse.
   * @param protected_branches Branch patterns excluded from consideration.
   * @param protected_branch_excludes Patterns that lift protection.
   * @return Branch names that are likely safe to prune.
   */
  std::vector<std::string> detect_stray_branches(
      const std::string &owner, const std::string &repo,
      const std::string &default_branch,
      const std::vector<std::string> &branches,
      const std::vector<std::string> &protected_branches = {},
      const std::vector<std::string> &protected_branch_excludes = {});

  /**
   * Perform a single HTTP request to list branches for a repository. Intended
   * for tests that must avoid pagination and extra metadata calls.
   *
   * @param owner_repo Repository in "owner/repo" format.
   * @param per_page Maximum number of branches to request (default 100).
   * @return List of branch names from a single page.
   */
  std::vector<std::string> list_branches_single(const std::string &owner_repo,
                                                int per_page = 100);

  /**
   * Delete branches whose associated pull request was closed or merged and
   * whose name begins with the given prefix.
   *
   * @param owner Repository owner.
   * @param repo Repository name.
   * @param prefix Branch name prefix to match for deletion.
   * @param protected_branches Glob patterns for branches that must not be
   *        deleted.
   * @param protected_branch_excludes Patterns that override protections.
   * @return Branch names successfully deleted from the repository.
   */
  std::vector<std::string> cleanup_branches(
      const std::string &owner, const std::string &repo,
      const std::string &prefix,
      const std::vector<std::string> &protected_branches = {},
      const std::vector<std::string> &protected_branch_excludes = {});

  /**
   * Close or delete branches that have diverged from the repository's default
   * branch.
   *
   * @param owner Repository owner.
   * @param repo Repository name.
   * @param protected_branches Glob patterns for branches that must not be
   *        removed.
   * @param protected_branch_excludes Patterns that override protections.
   */
  void close_dirty_branches(
      const std::string &owner, const std::string &repo,
      const std::vector<std::string> &protected_branches = {},
      const std::vector<std::string> &protected_branch_excludes = {});

  /// Snapshot of GitHub rate limit information for the authenticated token.
  struct RateLimitStatus {
    long limit{0};
    long remaining{0};
    long used{0};
    std::chrono::seconds reset_after{0};
  };

  /**
   * Retrieve the current GitHub rate limit status for the core REST resource.
   *
   * @param max_attempts Number of attempts to query `/rate_limit` before
   *        giving up (minimum of one).
   * @return Populated status snapshot when the endpoint succeeds;
   * `std::nullopt` if the probe fails or returns malformed data.
   */
  std::optional<RateLimitStatus> rate_limit_status(int max_attempts = 1);

private:
  mutable std::mutex mutex_;
  std::vector<std::string> tokens_;
  size_t token_index_{0};
  std::unique_ptr<HttpClient> http_;
  std::unordered_set<std::string> include_repos_;
  std::unordered_set<std::string> exclude_repos_;
  std::string api_base_;
  bool dry_run_{false};

  struct CachedResponse {
    std::string etag;
    std::string body;
    std::vector<std::string> headers;
  };
  std::unordered_map<std::string, CachedResponse> cache_;
  std::string cache_file_;

  int required_approvals_{0};
  bool require_status_success_{false};
  bool require_mergeable_state_{false};

  int delay_ms_;
  std::chrono::steady_clock::time_point last_request_;
  bool allow_delete_base_branch_{false};

  bool repo_allowed(const std::string &owner, const std::string &repo) const;
  void enforce_delay();
  bool handle_rate_limit(const HttpResponse &resp);
  HttpResponse get_with_cache_locked(const std::string &url,
                                     const std::vector<std::string> &headers);
  void load_cache_locked();
  void save_cache_locked();
  std::optional<PullRequestMetadata>
  pull_request_metadata_locked(const std::string &owner,
                               const std::string &repo, int pr_number);
  bool merge_pull_request_internal(const std::string &owner,
                                   const std::string &repo, int pr_number,
                                   const PullRequestMetadata *metadata);
};

/** Minimal GitHub GraphQL API client used for querying pull requests. */
class GitHubGraphQLClient {
public:
  /**
   * Construct a client using the provided tokens.
   *
   * @param tokens Personal access tokens used for authenticated requests.
   * @param timeout_ms Request timeout in milliseconds for GraphQL operations.
   * @param api_base Base URL for the GitHub API endpoints.
   */
  explicit GitHubGraphQLClient(std::vector<std::string> tokens,
                               int timeout_ms = 30000,
                               std::string api_base = "https://api.github.com");

  /**
   * List pull requests for a repository using GraphQL.
   *
   * @param owner Repository owner.
   * @param repo Repository name.
   * @param include_merged Include merged pull requests when true.
   * @param per_page Number of pull requests to fetch (max 100).
   * @return List of pull request summaries retrieved from the GraphQL API.
   */
  std::vector<PullRequest> list_pull_requests(const std::string &owner,
                                              const std::string &repo,
                                              bool include_merged = false,
                                              int per_page = 50);

private:
  std::vector<std::string> tokens_;
  size_t token_index_{0};
  int timeout_ms_;
  std::string api_base_;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_GITHUB_CLIENT_HPP
