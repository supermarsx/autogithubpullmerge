#ifndef AUTOGITHUBPULLMERGE_TUI_HPP
#define AUTOGITHUBPULLMERGE_TUI_HPP

#if defined(__CPPCHECK__)
// For static analysis, avoid hard failure; provide opaque WINDOW type.
struct _win_st;
using WINDOW = _win_st;
#elif __has_include(<curses.h>)
#include <curses.h>
#elif __has_include(<ncurses.h>)
#include <ncurses.h>
#elif __has_include(<ncurses/curses.h>)
#include <ncurses/curses.h>
#else
// As a last resort, provide minimal opaque type for declarations.
struct _win_st;
using WINDOW = _win_st;
#endif

#include "github_client.hpp"
#include "github_poller.hpp"
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace agpm {

/**
 * Minimal curses-based text user interface for interacting with repositories.
 */
class Tui {
public:
  /**
   * Construct a TUI bound to a GitHub client and poller.
   *
   * @param client GitHub API client used for interactive actions triggered by
   *        the user.
   * @param poller Poller providing periodic updates to display.
   * @param log_limit Maximum number of log messages to keep in memory.
   */
  Tui(GitHubClient &client, GitHubPoller &poller, std::size_t log_limit = 200);
  ~Tui();

  /// Initialize the curses library and windows.
  void init();

  /// Main interactive loop.
  void run();

  /// Clean up curses state.
  void cleanup();

  /**
   * Update the displayed pull requests.
   *
   * @param prs Latest list of pull requests to render.
   */
  void update_prs(const std::vector<PullRequest> &prs);

  /// Draw the interface once.
  void draw();

  /**
   * Handle a single key press.
   *
   * @param ch Character code received from curses.
   */
  void handle_key(int ch);

  /// Access collected log messages.
  const std::vector<std::string> &logs() const { return logs_; }

  /**
   * Check whether the TUI has been successfully initialized.
   *
   * @return `true` if curses has been initialized and windows created.
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
   * @param cmd Function returning the exit code after opening the URL. The
   *        callable receives the URL string and should return zero on success.
   */
  void set_open_cmd(std::function<int(const std::string &)> cmd) {
    open_cmd_ = std::move(cmd);
  }

  /// Enable or disable interactive hotkeys at runtime.
  void set_hotkeys_enabled(bool enabled) { hotkeys_enabled_ = enabled; }

  /**
   * Override the configured hotkey bindings.
   *
   * @param bindings Mapping from action name to binding specification string.
   *        Each string may contain comma-separated key descriptors such as
   *        `ctrl+c`.
   */
  void configure_hotkeys(
      const std::unordered_map<std::string, std::string> &bindings);

private:
  void log(const std::string &msg);
  struct HotkeyBinding {
    int key;
    std::string label;
  };
  void initialize_default_hotkeys();
  void clear_action_bindings(const std::string &action);
  void set_bindings_for_action(const std::string &action,
                               const std::vector<HotkeyBinding> &bindings);

  GitHubClient &client_;
  GitHubPoller &poller_;
  std::vector<PullRequest> prs_;
  std::vector<std::string> logs_;
  std::size_t log_limit_;
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
  bool hotkeys_enabled_{true};
  std::vector<std::string> hotkey_help_order_;
  std::unordered_map<std::string, std::vector<HotkeyBinding>> action_bindings_;
  std::unordered_map<int, std::string> key_to_action_;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_TUI_HPP
