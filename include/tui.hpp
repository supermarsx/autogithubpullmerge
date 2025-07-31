#ifndef AUTOGITHUBPULLMERGE_TUI_HPP
#define AUTOGITHUBPULLMERGE_TUI_HPP

#include "github_client.hpp"
#include <vector>

namespace agpm {

/**
 * Minimal ncurses-based text user interface.
 */
class Tui {
public:
  /// Initialize curses.
  Tui();
  /// End curses on destruction.
  ~Tui();

  /** Run the UI displaying a list of pull requests.
   * If the list is empty the UI initializes and exits immediately.
   */
  int run(const std::vector<PullRequest> &prs);
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_TUI_HPP
