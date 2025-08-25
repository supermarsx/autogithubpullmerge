#ifndef AUTOGITHUBPULLMERGE_GITHUB_CLIENT_HPP
#define AUTOGITHUBPULLMERGE_GITHUB_CLIENT_HPP

#include <chrono>
#include <curl/curl.h>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace agpm {

/** Simple HTTP response container. */
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
   * @param url Request URL
   * @param headers Additional request headers
   * @return Response body
   */
  virtual std::string get(const std::string &url,
                          const std::vector<std::string> &headers) = 0;

  /**
   * Perform a HTTP GET request returning both body and response headers.
   *
   * @param url Request URL
   * @param headers Additional request headers
   * @return Response body, headers and status code
   */
  virtual HttpResponse
  get_with_headers(const std::string &url,
                   const std::vector<std::string> &headers) {
    return {get(url, headers), {}, 200};
  }

  /**
   * Perform a HTTP PUT request.
   *
   * @param url Request URL
   * @param data Request body
   * @param headers Additional request headers
   * @return Response body
   */
  virtual std::string put(const std::string &url, const std::string &data,
                          const std::vector<std::string> &headers) = 0;

  /**
   * Perform a HTTP DELETE request.
   *
   * @param url Request URL
   * @param headers Additional request headers
   * @return Response body
   */
  virtual std::string del(const std::string &url,
                          const std::vector<std::string> &headers) = 0;
};

/**
 * RAII wrapper for a CURL easy handle.
 *
 * Ensures global CURL initialization occurs once.
 */
class CurlHandle {
public:
  CurlHandle();
  ~CurlHandle();
  CurlHandle(const CurlHandle &) = delete;
  CurlHandle &operator=(const CurlHandle &) = delete;
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
   * @param timeout_ms Request timeout in milliseconds
   * @param download_limit Maximum download rate in bytes per second (0 =
   * unlimited)
   * @param upload_limit Maximum upload rate in bytes per second (0 = unlimited)
   * @param max_download Maximum cumulative download in bytes (0 = unlimited)
   * @param max_upload Maximum cumulative upload in bytes (0 = unlimited)
   */
  explicit CurlHttpClient(long timeout_ms = 30000,
                          curl_off_t download_limit = 0,
                          curl_off_t upload_limit = 0,
                          curl_off_t max_download = 0,
                          curl_off_t max_upload = 0);

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

  /// @copydoc HttpClient::del()
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override;

  /// Total bytes downloaded so far.
  curl_off_t total_downloaded() const { return total_downloaded_; }

  /// Total bytes uploaded so far.
  curl_off_t total_uploaded() const { return total_uploaded_; }

private:
  CurlHandle curl_;
  long timeout_ms_;
  curl_off_t download_limit_;
  curl_off_t upload_limit_;
  curl_off_t max_download_;
  curl_off_t max_upload_;
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

/** Simple GitHub API client. */
class GitHubClient {
public:
  /**
   * Construct a GitHub API client.
   *
   * @param token Personal access token
   * @param http Optional HTTP client implementation
   * @param include_repos Repositories to include
   * @param exclude_repos Repositories to exclude
   * @param delay_ms Minimum delay between requests in milliseconds
   * @param timeout_ms HTTP request timeout in milliseconds
   * @param max_retries Number of retry attempts for transient failures
   */
  explicit GitHubClient(std::vector<std::string> tokens,
                        std::unique_ptr<HttpClient> http = nullptr,
                        std::unordered_set<std::string> include_repos = {},
                        std::unordered_set<std::string> exclude_repos = {},
                        int delay_ms = 0, int timeout_ms = 30000,
                        int max_retries = 3,
                        std::string api_base = "https://api.github.com",
                        bool dry_run = false);

  /// Set minimum delay between HTTP requests in milliseconds.
  void set_delay_ms(int delay_ms);

  /// Set required approvals before merging.
  void set_required_approvals(int n) { required_approvals_ = n; }

  /// Set whether successful status checks are required before merging.
  void set_require_status_success(bool v) { require_status_success_ = v; }

  /// Set whether a PR must be mergeable before merging.
  void set_require_mergeable_state(bool v) { require_mergeable_state_ = v; }

  /**
   * List repositories accessible to the authenticated user.
   *
   * @return List of repository owner/name pairs
   */
  std::vector<std::pair<std::string, std::string>> list_repositories();

  /**
   * List pull requests for a repository.
   *
   * @param owner Repository owner
   * @param repo Repository name
   * @param include_merged Include merged pull requests when true
   * @param per_page Number of pull requests to fetch per page
   * @param since Only include pull requests updated since this time
   * @return List of pull request summaries
   */
  std::vector<PullRequest>
  list_pull_requests(const std::string &owner, const std::string &repo,
                     bool include_merged = false, int per_page = 50,
                     std::chrono::seconds since = std::chrono::seconds{0});

  /**
   * Merge a pull request.
   *
   * @param owner Repository owner
   * @param repo Repository name
   * @param pr_number Pull request number
   * @return True if merge was successful
   */
  bool merge_pull_request(const std::string &owner, const std::string &repo,
                          int pr_number);

  /**
   * List branch names for a repository excluding the default branch.
   *
   * @param owner Repository owner
   * @param repo Repository name
   * @return All non-default branch names
   */
  std::vector<std::string> list_branches(const std::string &owner,
                                         const std::string &repo);

  /**
   * Delete branches whose associated pull request was closed or merged and
   * whose name begins with the given prefix.
   *
   * @param owner Repository owner
   * @param repo Repository name
   * @param prefix Branch name prefix to match for deletion
   * @param protected_branches Glob patterns for branches that must not be
   *        deleted
   * @param protected_branch_excludes Patterns that override protections
   */
  void cleanup_branches(
      const std::string &owner, const std::string &repo,
      const std::string &prefix,
      const std::vector<std::string> &protected_branches = {},
      const std::vector<std::string> &protected_branch_excludes = {});

  /**
   * Close or delete branches that have diverged from the repository's default
   * branch.
   *
   * @param owner Repository owner
   * @param repo Repository name
   * @param protected_branches Glob patterns for branches that must not be
   *        removed
   * @param protected_branch_excludes Patterns that override protections
   */
  void close_dirty_branches(
      const std::string &owner, const std::string &repo,
      const std::vector<std::string> &protected_branches = {},
      const std::vector<std::string> &protected_branch_excludes = {});

private:
  std::vector<std::string> tokens_;
  size_t token_index_{0};
  std::unique_ptr<HttpClient> http_;
  std::unordered_set<std::string> include_repos_;
  std::unordered_set<std::string> exclude_repos_;
  std::string api_base_;
  bool dry_run_{false};

  int required_approvals_{0};
  bool require_status_success_{false};
  bool require_mergeable_state_{false};

  int delay_ms_;
  std::chrono::steady_clock::time_point last_request_;

  bool repo_allowed(const std::string &owner, const std::string &repo) const;
  void enforce_delay();
  bool handle_rate_limit(const HttpResponse &resp);
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_GITHUB_CLIENT_HPP
