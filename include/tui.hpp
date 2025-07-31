#ifndef AUTOGITHUBPULLMERGE_TUI_HPP
#define AUTOGITHUBPULLMERGE_TUI_HPP

#include <curses.h>

namespace agpm {

/**
 * Minimal ncurses-based text user interface.
 */
class Tui {
public:
  /// Initialize the ncurses library.
  void init();

  /// Display a placeholder window until a key is pressed.
  void run();

  /// Clean up ncurses state.
  void cleanup();
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_TUI_HPP
