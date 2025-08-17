#ifndef AUTOGITHUBPULLMERGE_APP_HPP
#define AUTOGITHUBPULLMERGE_APP_HPP

#include "cli.hpp"
#include "config.hpp"
#include "github_client.hpp"
#include <memory>
#include <vector>

namespace agpm {

/** Main application entry point. */
class App {
public:
  /**
   * Run the application with the given command line arguments.
   *
   * @param argc Argument count
   * @param argv Argument values
   * @return 0 on success, non‐zero otherwise
   */
  int run(int argc, char **argv);

  /// Inject a custom HTTP client for testing.
  void set_http_client(std::unique_ptr<HttpClient> http) {
    http_client_ = std::move(http);
  }

  /** Retrieve the parsed command line options. */
  const CliOptions &options() const { return options_; }

  /** Retrieve the loaded configuration. */
  const Config &config() const { return config_; }

  /// Repositories explicitly included via the CLI.
  const std::vector<std::string> &include_repos() const {
    return include_repos_;
  }

  /// Repositories explicitly excluded via the CLI.
  const std::vector<std::string> &exclude_repos() const {
    return exclude_repos_;
  }

private:
  CliOptions options_;
  Config config_;
  std::vector<std::string> include_repos_;
  std::vector<std::string> exclude_repos_;
  std::unique_ptr<HttpClient> http_client_;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_APP_HPP
