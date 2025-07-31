#include "tui.hpp"
#include <curses.h>

namespace agpm {

Tui::Tui() { initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); start_color(); use_default_colors(); init_pair(1, COLOR_CYAN, -1); }

Tui::~Tui() { endwin(); }

int Tui::run(const std::vector<PullRequest> &prs) {
  if (prs.empty()) {
    return 0;
  }
  int highlight = 0;
  int offset = 0;
  int ch = 0;
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  while ((ch = getch()) != 'q') {
    if (ch == KEY_DOWN && highlight < static_cast<int>(prs.size()) - 1)
      ++highlight;
    else if (ch == KEY_UP && highlight > 0)
      --highlight;
    if (highlight < offset)
      offset = highlight;
    else if (highlight >= offset + rows)
      offset = highlight - rows + 1;
    clear();
    for (int i = 0; i < rows && i + offset < static_cast<int>(prs.size()); ++i) {
      int idx = i + offset;
      if (idx == highlight)
        attron(COLOR_PAIR(1) | A_BOLD);
      mvprintw(i, 0, "#%d %s", prs[idx].number, prs[idx].title.c_str());
      if (idx == highlight)
        attroff(COLOR_PAIR(1) | A_BOLD);
    }
    refresh();
  }
  return 0;
}

} // namespace agpm
