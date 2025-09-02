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
#include <cstdlib>
#include <spdlog/spdlog.h>
#include <stdexcept>
#if defined(_WIN32)
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

namespace agpm {

Tui::Tui(GitHubClient &client, GitHubPoller &poller, std::size_t log_limit)
    : client_(client), poller_(poller), log_limit_(log_limit) {
  open_cmd_ = [](const std::string &url) {
#if defined(_WIN32)
    std::string cmd = "start \"\" \"" + url + "\"";
#elif defined(__APPLE__)
    std::string cmd = "open \"" + url + "\"";
#else
    std::string cmd = "xdg-open \"" + url + "\"";
#endif
    return std::system(cmd.c_str());
  };
  poller_.set_pr_callback(
      [this](const std::vector<PullRequest> &prs) { update_prs(prs); });
  poller_.set_log_callback([this](const std::string &msg) { log(msg); });
}

void Tui::init() {
  // Ensure all standard streams are attached to a real terminal. macOS
  // builds in CI environments may have one or more redirected which can
  // cause ncurses to crash with a bus error when initializing.
  if (!isatty(fileno(stdout)) || !isatty(fileno(stdin)) ||
      !isatty(fileno(stderr))) {
    return;
  }
  if (initscr() == nullptr) {
    throw std::runtime_error("Failed to initialize curses");
  }
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
  initialized_ = true;
}

void Tui::update_prs(const std::vector<PullRequest> &prs) {
  prs_ = prs;
  if (selected_ >= static_cast<int>(prs_.size())) {
    selected_ = prs_.empty() ? 0 : static_cast<int>(prs_.size()) - 1;
  }
}

void Tui::log(const std::string &msg) {
  logs_.push_back(msg);
  if (logs_.size() > log_limit_) {
    logs_.erase(logs_.begin(), logs_.begin() + (logs_.size() - log_limit_));
  }
}

void Tui::draw() {
  if (!initialized_)
    return;
  int h, w;
  getmaxyx(stdscr, h, w);
  int log_h = h / 3;
  int help_w = w / 3;
  if (h != last_h_ || w != last_w_) {
    last_h_ = h;
    last_w_ = w;
    if (pr_win_)
      delwin(pr_win_);
    if (log_win_)
      delwin(log_win_);
    if (help_win_)
      delwin(help_win_);
    pr_win_ = newwin(h - log_h, w, 0, 0);
    log_win_ = newwin(log_h, w - help_w, h - log_h, 0);
    help_win_ = newwin(log_h, help_w, h - log_h, w - help_w);
    if (detail_win_) {
      delwin(detail_win_);
      detail_win_ = nullptr;
    }
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
    mvwprintw(pr_win_, 1 + i, 1, "%s/%s #%d %s", prs_[i].owner.c_str(),
              prs_[i].repo.c_str(), prs_[i].number, prs_[i].title.c_str());
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
  mvwprintw(help_win_, 3, 1, "o - Open PR");
  mvwprintw(help_win_, 4, 1, "ENTER/d - Details");
  mvwprintw(help_win_, 5, 1, "q - Quit");
  if (has_colors())
    wattroff(help_win_, COLOR_PAIR(3));
  wrefresh(help_win_);

  if (detail_visible_) {
    int dh = h / 2;
    int dw = w / 2;
    if (!detail_win_)
      detail_win_ = newwin(dh, dw, (h - dh) / 2, (w - dw) / 2);
    werase(detail_win_);
    box(detail_win_, 0, 0);
    mvwprintw(detail_win_, 0, 2, "PR Details");
    const auto &pr = prs_[selected_];
    mvwprintw(detail_win_, 1, 1, "%s/%s #%d", pr.owner.c_str(), pr.repo.c_str(),
              pr.number);
    mvwprintw(detail_win_, 2, 1, "%s", detail_text_.c_str());
    mvwprintw(detail_win_, dh - 2, 1, "Press ENTER or d to close");
    wrefresh(detail_win_);
  } else if (detail_win_) {
    delwin(detail_win_);
    detail_win_ = nullptr;
  }
}

void Tui::handle_key(int ch) {
  if (!initialized_)
    return;
  spdlog::debug("Key pressed: {}", ch);
  switch (ch) {
  case 'q':
    spdlog::info("Quit requested");
    running_ = false;
    break;
  case 'r':
    spdlog::info("Manual refresh requested");
    poller_.poll_now();
    break;
  case 'm':
    if (selected_ < static_cast<int>(prs_.size())) {
      const auto &pr = prs_[selected_];
      spdlog::info("Merge requested for PR #{}", pr.number);
      if (client_.merge_pull_request(pr.owner, pr.repo, pr.number)) {
        log("Merged PR #" + std::to_string(pr.number));
      }
    }
    break;
  case 'o':
    if (selected_ < static_cast<int>(prs_.size())) {
      const auto &pr = prs_[selected_];
      std::string url = "https://github.com/" + pr.owner + "/" + pr.repo +
                        "/pull/" + std::to_string(pr.number);
      open_cmd_(url);
    }
    break;
  case 'd':
  case '\n':
    if (detail_visible_) {
      detail_visible_ = false;
    } else if (selected_ < static_cast<int>(prs_.size())) {
      const auto &pr = prs_[selected_];
      detail_text_ = pr.title;
      detail_visible_ = true;
    }
    break;
  case KEY_UP:
    if (selected_ > 0) {
      --selected_;
      spdlog::debug("Moved selection up to {}", selected_);
    }
    break;
  case KEY_DOWN:
    if (selected_ + 1 < static_cast<int>(prs_.size())) {
      ++selected_;
      spdlog::debug("Moved selection down to {}", selected_);
    }
    break;
  default:
    spdlog::debug("Unhandled key: {}", ch);
    break;
  }
}

void Tui::run() {
  if (!initialized_)
    return;
  running_ = true;
  while (running_) {
    draw();
    int ch = getch();
    handle_key(ch);
  }
}

void Tui::cleanup() {
  if (!initialized_)
    return;
  if (pr_win_)
    delwin(pr_win_);
  if (log_win_)
    delwin(log_win_);
  if (help_win_)
    delwin(help_win_);
  if (detail_win_)
    delwin(detail_win_);
  endwin();
  initialized_ = false;
}

} // namespace agpm
