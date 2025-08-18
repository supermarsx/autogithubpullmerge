#ifndef AUTOGITHUBPULLMERGE_GITHUB_CLIENT_HPP
#define AUTOGITHUBPULLMERGE_GITHUB_CLIENT_HPP

#include <chrono>
#include <curl/curl.h>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
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
   */
  explicit CurlHttpClient(long timeout_ms = 30000);

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

private:
  CurlHandle curl_;
  long timeout_ms_;
};

/// Representation of a GitHub pull request.
struct PullRequest {
  int number;          ///< PR number
  std::string title;   ///< PR title
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
  explicit GitHubClient(std::string token,
                        std::unique_ptr<HttpClient> http = nullptr,
                        std::vector<std::string> include_repos = {},
                        std::vector<std::string> exclude_repos = {},
                        int delay_ms = 0, int timeout_ms = 30000,
                        int max_retries = 3);

  /// Set minimum delay between HTTP requests in milliseconds.
  void set_delay_ms(int delay_ms);

  /**
   * List pull requests for a repository.
   *
   * @param owner Repository owner
   * @param repo Repository name
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
   * Delete branches whose associated pull request was closed or merged and
   * whose name begins with the given prefix.
   */
  void cleanup_branches(const std::string &owner, const std::string &repo,
                        const std::string &prefix);

  /**
   * Close or delete branches that have diverged from the repository's default
   * branch.
   */
  void close_dirty_branches(const std::string &owner, const std::string &repo);

private:
  std::string token_;
  std::unique_ptr<HttpClient> http_;
  std::vector<std::string> include_repos_;
  std::vector<std::string> exclude_repos_;

  int delay_ms_;
  std::chrono::steady_clock::time_point last_request_;

  bool repo_allowed(const std::string &owner, const std::string &repo) const;
  void enforce_delay();
  bool handle_rate_limit(const HttpResponse &resp);
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_GITHUB_CLIENT_HPP
