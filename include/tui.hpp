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
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace agpm {

/**
 * Minimal curses-based text user interface.
 */
class Tui {
public:
  /**
   * Construct a TUI bound to a GitHub client and poller.
   *
   * @param client GitHub API client used for interactive actions
   * @param poller Poller providing periodic updates
   */
  Tui(GitHubClient &client, GitHubPoller &poller);

  /// Initialize the curses library and windows.
  void init();

  /// Main interactive loop.
  void run();

  /// Clean up curses state.
  void cleanup();

  /**
   * Update the displayed pull requests.
   *
   * @param prs Latest list of pull requests to render
   */
  void update_prs(const std::vector<PullRequest> &prs);

  /// Draw the interface once.
  void draw();

  /**
   * Handle a single key press.
   *
   * @param ch Character code received from curses
   */
  void handle_key(int ch);

  /// Access collected log messages.
  const std::vector<std::string> &logs() const { return logs_; }

  /**
   * Check whether the TUI has been successfully initialized.
   *
   * @return true if curses has been initialized and windows created
   */
  bool initialized() const { return initialized_; }

  /// Access the main pull request window (primarily for tests).
  WINDOW *pr_win() const { return pr_win_; }

  /// Access the help window (primarily for tests).
  WINDOW *help_win() const { return help_win_; }

  /// Access the detail window (primarily for tests).
  WINDOW *detail_win() const { return detail_win_; }

  /**
   * Override the command used to open URLs. Intended for tests.
   *
   * @param cmd Function returning the exit code after opening the URL
   */
  void set_open_cmd(std::function<int(const std::string &)> cmd) {
    open_cmd_ = std::move(cmd);
  }

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
  WINDOW *detail_win_{nullptr};
  bool detail_visible_{false};
  std::string detail_text_;
  std::function<int(const std::string &)> open_cmd_;
  bool running_{false};
  bool initialized_{false};
  int last_h_{0}; ///< Cached terminal height for resize detection.
  int last_w_{0}; ///< Cached terminal width for resize detection.
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_TUI_HPP
