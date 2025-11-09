/**
 * @file mcp_server.hpp
 * @brief Model Context Protocol (MCP) server and backend for
 * autogithubpullmerge.
 *
 * Declares the MCP server, backend interfaces, and runner for automation
 * integrations.
 */

#ifndef AUTOGITHUBPULLMERGE_MCP_SERVER_HPP
#define AUTOGITHUBPULLMERGE_MCP_SERVER_HPP

#include "github_client.hpp"
#include <atomic>
#include <functional>
#include <iosfwd>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace agpm {

/**
 * Abstract backend used by the MCP server to interact with repositories.
 */
class McpBackend {
public:
  virtual ~McpBackend() = default;

  /// Return repositories visible to the integration layer.
  virtual std::vector<std::pair<std::string, std::string>>
  list_repositories() = 0;

  /// List pull requests for a repository.
  virtual std::vector<PullRequest> list_pull_requests(const std::string &owner,
                                                      const std::string &repo,
                                                      bool include_merged) = 0;

  /// List branch names for a repository.
  virtual std::vector<std::string> list_branches(const std::string &owner,
                                                 const std::string &repo) = 0;

  /// Merge a pull request by number.
  virtual bool merge_pull_request(const std::string &owner,
                                  const std::string &repo, int pr_number) = 0;

  /// Close a pull request without merging.
  virtual bool close_pull_request(const std::string &owner,
                                  const std::string &repo, int pr_number) = 0;

  /// Delete a branch.
  virtual bool delete_branch(const std::string &owner, const std::string &repo,
                             const std::string &branch) = 0;
};

/**
 * GitHub-backed implementation of the MCP server backend.
 */
class GitHubMcpBackend : public McpBackend {
public:
  GitHubMcpBackend(
      GitHubClient &client,
      std::vector<std::pair<std::string, std::string>> repositories = {},
      std::vector<std::string> protected_branches = {},
      std::vector<std::string> protected_branch_excludes = {});

  std::vector<std::pair<std::string, std::string>> list_repositories() override;

  std::vector<PullRequest> list_pull_requests(const std::string &owner,
                                              const std::string &repo,
                                              bool include_merged) override;

  std::vector<std::string> list_branches(const std::string &owner,
                                         const std::string &repo) override;

  bool merge_pull_request(const std::string &owner, const std::string &repo,
                          int pr_number) override;

  bool close_pull_request(const std::string &owner, const std::string &repo,
                          int pr_number) override;

  bool delete_branch(const std::string &owner, const std::string &repo,
                     const std::string &branch) override;

private:
  GitHubClient &client_;
  std::vector<std::pair<std::string, std::string>> repositories_;
  std::vector<std::string> protected_branches_;
  std::vector<std::string> protected_branch_excludes_;
  std::mutex mutex_;
};

/**
 * Minimal JSON-RPC server implementing the Model Context Protocol commands
 * required for automation integrations.
 */
class McpServer {
public:
  explicit McpServer(McpBackend &backend);

  /// Process a single JSON-RPC request payload.
  nlohmann::json handle_request(const nlohmann::json &request);

  /// Run the server loop, reading JSON objects line-by-line from @p input.
  void run(std::istream &input, std::ostream &output);

  /// Reset the server to an accepting state (used when restarting the
  /// listener).
  void reset();

  /// Check whether the server should continue processing requests.
  bool running() const;

  using EventCallback = std::function<void(const std::string &)>;

  /// Register a callback invoked whenever the server records an event.
  void set_event_callback(EventCallback cb);

  friend class McpServerRunner;

private:
  nlohmann::json make_error(const nlohmann::json &id, int code,
                            const std::string &message) const;
  nlohmann::json make_result(const nlohmann::json &id,
                             const nlohmann::json &result) const;
  void emit_event(const std::string &message) const;

  McpBackend &backend_;
  std::atomic<bool> running_{true};
  mutable std::mutex event_mutex_;
  EventCallback event_callback_;
};

struct McpServerOptions {
  std::string bind_address{"127.0.0.1"};
  int port{7332};
  int backlog{16};
  int max_clients{4};
};

class McpServerRunner {
public:
  using EventSink = std::function<void(const std::string &)>;

  McpServerRunner(McpServer &server, McpServerOptions options);
  ~McpServerRunner();

  /**
   * @brief Start the MCP server runner in a background thread.
   */
  void start();
  /**
   * @brief Stop the MCP server runner and join the background thread.
   */
  void stop();
  /**
   * @brief Check if the MCP server runner is currently running.
   * @return True if running, false otherwise.
   */
  bool running() const { return running_; }

  /**
   * @brief Register a callback to receive event messages from the server
   * runner.
   * @param sink Callback function to handle event messages.
   */
  void set_event_sink(EventSink sink);

private:
  void run();
  void emit(const std::string &message);
  void close_listener();

  McpServer &server_;
  McpServerOptions options_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};
  std::mutex sink_mutex_;
  EventSink sink_;
#ifdef _WIN32
  using SocketHandle = SOCKET;
  SocketHandle listener_{INVALID_SOCKET};
#else
  using SocketHandle = int;
  SocketHandle listener_{-1};
#endif
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_MCP_SERVER_HPP
