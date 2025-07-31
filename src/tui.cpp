#include "tui.hpp"
#include <curses.h>

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
