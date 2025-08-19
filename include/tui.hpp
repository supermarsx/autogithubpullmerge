#ifndef AUTOGITHUBPULLMERGE_TUI_HPP
#define AUTOGITHUBPULLMERGE_TUI_HPP

#if __has_include(<curses.h>)
#include <curses.h>
#elif __has_include(<ncurses.h>)
#include <ncurses.h>
#elif __has_include(<ncurses/curses.h>)
#include <ncurses/curses.h>
#else
#error "curses.h not found"
#endif

#include "github_client.hpp"
#include "github_poller.hpp"
#include <string>
#include <vector>

namespace agpm {

/**
 * Minimal curses-based text user interface.
 */
class Tui {
public:
  /// Construct a TUI bound to a GitHub client and poller.
  Tui(GitHubClient &client, GitHubPoller &poller);

  /// Initialize the curses library and windows.
  void init();

  /// Main interactive loop.
  void run();

  /// Clean up curses state.
  void cleanup();

  /// Update the displayed pull requests.
  void update_prs(const std::vector<PullRequest> &prs);

  /// Draw the interface once.
  void draw();

  /// Handle a single key press.
  void handle_key(int ch);

  /// Access collected log messages.
  const std::vector<std::string> &logs() const { return logs_; }

private:
  void log(const std::string &msg);

  GitHubClient &client_;
  GitHubPoller &poller_;
  std::vector<PullRequest> prs_;
  std::vector<std::string> logs_;
  int selected_{0};
  WINDOW *pr_win_{nullptr};
  WINDOW *log_win_{nullptr};
  WINDOW *help_win_{nullptr};
  bool running_{false};
  int last_h_{0}; ///< Cached terminal height for resize detection.
  int last_w_{0}; ///< Cached terminal width for resize detection.
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_TUI_HPP
