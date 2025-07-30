#ifndef AUTOGITHUBPULLMERGE_APP_HPP
#define AUTOGITHUBPULLMERGE_APP_HPP

#include "cli.hpp"

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

private:
  CliOptions options_;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_APP_HPP
