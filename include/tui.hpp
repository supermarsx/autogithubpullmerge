#ifndef AUTOGITHUBPULLMERGE_TUI_HPP
#define AUTOGITHUBPULLMERGE_TUI_HPP

#if __has_include(<curses.h>)
#include <curses.h>
#elif __has_include(<ncurses.h>)
#include <ncurses.h>
#elif __has_include(<ncurses/curses.h>)
#include <ncurses/curses.h>
#elif __has_include("../libs/pdcurses/curses.h")
#include "../libs/pdcurses/curses.h"
#else
#error "curses.h not found"
#endif

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
