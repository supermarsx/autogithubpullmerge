#include "tui.hpp"

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

void Tui::init() {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);
}

void Tui::run() {
  clear();
  box(stdscr, 0, 0);
  mvprintw(1, 2, "AGPM running");
  refresh();
  getch();
}

void Tui::cleanup() { endwin(); }

} // namespace agpm
