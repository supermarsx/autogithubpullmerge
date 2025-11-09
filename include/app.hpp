/**
 * @file app.hpp
 * @brief Main application entry point and orchestrator for autogithubpullmerge.
 *
 * Declares the App class, which manages high-level application flow,
 * configuration loading, and CLI parsing for the autogithubpullmerge tool.
 */

#ifndef AUTOGITHUBPULLMERGE_APP_HPP
#define AUTOGITHUBPULLMERGE_APP_HPP

#include "cli.hpp"
#include "config.hpp"
#include <vector>

namespace agpm {

/**
 * Main application entry point responsible for orchestrating high level
 * application flow, configuration loading, and CLI parsing.
 */
class App {
public:
  /**
   * Run the application with the given command line arguments.
   *
   * @param argc Number of CLI arguments supplied to the executable.
   * @param argv Null-terminated array containing the raw CLI arguments.
   * @return Zero on success, non-zero when execution should terminate due to
   *         an error.
   */
  int run(int argc, char **argv);

  /**
   * Retrieve the parsed command line options.
   *
   * @return Immutable reference to the populated CLI options structure.
   */
  const CliOptions &options() const { return options_; }

  /**
   * Retrieve the loaded configuration.
   *
   * @return Immutable reference to the resolved configuration values.
   */
  const Config &config() const { return config_; }

  /**
   * Determine whether the application should exit immediately after
   * `run()` completes.
   *
   * @return `true` if the application should terminate, otherwise `false`.
   */
  bool should_exit() const { return should_exit_; }

  /**
   * Access the list of repositories explicitly included via the CLI.
   *
   * @return Immutable list of repository names to include.
   */
  const std::vector<std::string> &include_repos() const {
    return include_repos_;
  }

  /**
   * Access the list of repositories explicitly excluded via the CLI.
   *
   * @return Immutable list of repository names to exclude.
   */
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
