#ifndef AUTOGITHUBPULLMERGE_GITHUB_CLIENT_HPP
#define AUTOGITHUBPULLMERGE_GITHUB_CLIENT_HPP

#include <chrono>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace agpm {

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
   * Perform a HTTP PUT request.
   *
   * @param url Request URL
   * @param data Request body
   * @param headers Additional request headers
   * @return Response body
   */
  virtual std::string put(const std::string &url, const std::string &data,
                          const std::vector<std::string> &headers) = 0;
};

/** CURL-based HTTP client implementation. */
class CurlHttpClient : public HttpClient {
public:
  /// @copydoc HttpClient::get()
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override;

  /// @copydoc HttpClient::put()
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override;
};

/// Representation of a GitHub pull request.
struct PullRequest {
  int number;
  std::string title;
};

/** Simple GitHub API client. */
class GitHubClient {
public:
  /**
   * Construct a GitHub API client.
   *
   * @param token Personal access token
   * @param http Optional HTTP client implementation
   */
  explicit GitHubClient(std::string token,
                        std::unique_ptr<HttpClient> http = nullptr,
                        std::vector<std::string> include_repos = {},
                        std::vector<std::string> exclude_repos = {},
                        int delay_ms = 0);

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

private:
  std::string token_;
  std::unique_ptr<HttpClient> http_;
  std::vector<std::string> include_repos_;
  std::vector<std::string> exclude_repos_;

  int delay_ms_;
  std::chrono::steady_clock::time_point last_request_;

  bool repo_allowed(const std::string &repo) const;
  void enforce_delay();
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_GITHUB_CLIENT_HPP
