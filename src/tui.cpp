#include "tui.hpp"

#if __has_include(<curses.h>)
#include <curses.h>
#elif __has_include(<ncurses.h>)
#include <ncurses.h>
#elif __has_include(<ncurses/curses.h>)
#include <ncurses/curses.h>
#else
#error "curses.h not found"
#endif

namespace agpm {

Tui::Tui(GitHubClient &client, GitHubPoller &poller)
    : client_(client), poller_(poller) {
  poller_.set_pr_callback(
      [this](const std::vector<PullRequest> &prs) { update_prs(prs); });
  poller_.set_log_callback([this](const std::string &msg) { log(msg); });
}

void Tui::init() {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);
  if (has_colors()) {
    start_color();
#if defined(COLOR_PAIR)
    use_default_colors();
#endif
    init_pair(1, COLOR_CYAN, -1);   // highlight
    init_pair(2, COLOR_YELLOW, -1); // logs
    init_pair(3, COLOR_GREEN, -1);  // help text
  }
  refresh();
}

void Tui::update_prs(const std::vector<PullRequest> &prs) {
  prs_ = prs;
  if (selected_ >= static_cast<int>(prs_.size())) {
    selected_ = prs_.empty() ? 0 : static_cast<int>(prs_.size()) - 1;
  }
}

void Tui::log(const std::string &msg) { logs_.push_back(msg); }

void Tui::draw() {
  int h, w;
  getmaxyx(stdscr, h, w);
  int log_h = h / 3;
  int help_w = w / 3;
  if (!pr_win_) {
    pr_win_ = newwin(h - log_h, w, 0, 0);
    log_win_ = newwin(log_h, w - help_w, h - log_h, 0);
    help_win_ = newwin(log_h, help_w, h - log_h, w - help_w);
  }
  if (has_colors()) {
    wbkgd(pr_win_, COLOR_PAIR(0));
    wbkgd(log_win_, COLOR_PAIR(0));
    wbkgd(help_win_, COLOR_PAIR(0));
    redrawwin(pr_win_);
    redrawwin(log_win_);
    redrawwin(help_win_);
  }
  // PR window
  werase(pr_win_);
  box(pr_win_, 0, 0);
  mvwprintw(pr_win_, 0, 2, "Active PRs");
  int max_pr_lines = h - log_h - 2;
  for (int i = 0; i < static_cast<int>(prs_.size()) && i < max_pr_lines; ++i) {
    if (i == selected_) {
      wattron(pr_win_, COLOR_PAIR(1));
    }
    mvwprintw(pr_win_, 1 + i, 1, "#%d %s", prs_[i].number,
              prs_[i].title.c_str());
    if (i == selected_) {
      wattroff(pr_win_, COLOR_PAIR(1));
    }
  }
  wrefresh(pr_win_);

  // Log window
  werase(log_win_);
  box(log_win_, 0, 0);
  mvwprintw(log_win_, 0, 2, "Logs");
  int max_log_lines = log_h - 2;
  int start = logs_.size() > static_cast<size_t>(max_log_lines)
                  ? static_cast<int>(logs_.size()) - max_log_lines
                  : 0;
  for (int i = 0;
       start + i < static_cast<int>(logs_.size()) && i < max_log_lines; ++i) {
    wattron(log_win_, COLOR_PAIR(2));
    mvwprintw(log_win_, 1 + i, 1, "%s", logs_[start + i].c_str());
    wattroff(log_win_, COLOR_PAIR(2));
  }
  wrefresh(log_win_);

  // Help window
  werase(help_win_);
  box(help_win_, 0, 0);
  mvwprintw(help_win_, 0, 2, "Hotkeys");
  if (has_colors())
    wattron(help_win_, COLOR_PAIR(3));
  mvwprintw(help_win_, 1, 1, "r - Refresh");
  mvwprintw(help_win_, 2, 1, "m - Merge");
  mvwprintw(help_win_, 3, 1, "q - Quit");
  if (has_colors())
    wattroff(help_win_, COLOR_PAIR(3));
  wrefresh(help_win_);
}

void Tui::handle_key(int ch) {
  switch (ch) {
  case 'q':
    running_ = false;
    break;
  case 'r':
    poller_.poll_now();
    break;
  case 'm':
    if (selected_ < static_cast<int>(prs_.size())) {
      int num = prs_[selected_].number;
      if (client_.merge_pull_request("", "", num)) {
        log("Merged PR #" + std::to_string(num));
      }
    }
    break;
  case KEY_UP:
    if (selected_ > 0)
      --selected_;
    break;
  case KEY_DOWN:
    if (selected_ + 1 < static_cast<int>(prs_.size()))
      ++selected_;
    break;
  default:
    break;
  }
}

void Tui::run() {
  running_ = true;
  while (running_) {
    draw();
    int ch = getch();
    handle_key(ch);
  }
}

void Tui::cleanup() {
  if (pr_win_)
    delwin(pr_win_);
  if (log_win_)
    delwin(log_win_);
  if (help_win_)
    delwin(help_win_);
  endwin();
}

} // namespace agpm
