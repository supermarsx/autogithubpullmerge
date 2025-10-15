#ifndef AUTOGITHUBPULLMERGE_APP_HPP
#define AUTOGITHUBPULLMERGE_APP_HPP

#include "cli.hpp"
#include "config.hpp"
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

  /** Retrieve the parsed command line options. */
  const CliOptions &options() const { return options_; }

  /** Retrieve the loaded configuration. */
  const Config &config() const { return config_; }

  /** Whether the application should exit immediately after run(). */
  bool should_exit() const { return should_exit_; }

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
  bool should_exit_{false};
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_APP_HPP
