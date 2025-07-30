#ifndef AUTOGITHUBPULLMERGE_GITHUB_CLIENT_HPP
#define AUTOGITHUBPULLMERGE_GITHUB_CLIENT_HPP

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace agpm {

/** Interface for performing HTTP requests. */
class HttpClient {
public:
  virtual ~HttpClient() = default;
  virtual std::string get(const std::string &url,
                          const std::vector<std::string> &headers) = 0;
  virtual std::string put(const std::string &url, const std::string &data,
                          const std::vector<std::string> &headers) = 0;
};

/** CURL-based HTTP client implementation. */
class CurlHttpClient : public HttpClient {
public:
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override;
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
  explicit GitHubClient(std::string token,
                        std::unique_ptr<HttpClient> http = nullptr);

  std::vector<PullRequest> list_pull_requests(const std::string &owner,
                                              const std::string &repo);

  bool merge_pull_request(const std::string &owner, const std::string &repo,
                          int pr_number);

private:
  std::string token_;
  std::unique_ptr<HttpClient> http_;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_GITHUB_CLIENT_HPP
