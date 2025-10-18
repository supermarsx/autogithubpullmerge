#include "demo_tui.hpp"
#include "log.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#if defined(__CPPCHECK__)
struct _win_st;
using WINDOW = _win_st;
#elif __has_include(<curses.h>)
#include <curses.h>
#elif __has_include(<ncurses.h>)
#include <ncurses.h>
#elif __has_include(<ncurses/curses.h>)
#include <ncurses/curses.h>
#else
struct _win_st;
using WINDOW = _win_st;
#endif

#include <string>
#if defined(_WIN32)
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif
#include <vector>

namespace agpm {

namespace {
struct DemoPullRequest {
  std::string owner;
  std::string repo;
  int number;
  std::string title;
  std::string status;
};

struct DemoBranch {
  std::string name;
  std::string last_commit;
  std::string note;
};

void draw_header(int width) {
  mvhline(0, 0, ' ', width);
  mvprintw(0, 2, "autogithubpullmerge :: Demo Mode");
}

void draw_footer(int height, int width, bool focus_prs) {
  mvhline(height - 2, 0, ' ', width);
  mvprintw(height - 2, 2,
           "Controls: \u2191/\u2193 navigate  |  TAB switch pane  |  q quit");
  mvhline(height - 1, 0, ' ', width);
  mvprintw(height - 1, 2, "Focused: %s", focus_prs ? "Pull Requests" : "Branches");
}

void draw_pull_requests(const std::vector<DemoPullRequest> &prs, int selected,
                        int start_row, int rows, int width, bool focused) {
  mvhline(start_row, 0, ' ', width);
  mvprintw(start_row, 2, "Pull Requests");
  ++start_row;
  int start_index = 0;
  if (selected >= rows) {
    start_index = selected - rows + 1;
  }
  for (int i = 0; i < rows; ++i) {
    mvhline(start_row + i, 0, ' ', width);
  }
  for (int row = 0; row < rows &&
                   start_index + row < static_cast<int>(prs.size());
       ++row) {
    const auto &item = prs[start_index + row];
    if (start_index + row == selected) {
      attron(A_REVERSE | (focused ? A_BOLD : A_NORMAL));
    }
    mvprintw(start_row + row, 2, "#%d %-12s %-18s %-34s", item.number,
             (item.owner + "/" + item.repo).c_str(), item.status.c_str(),
             item.title.c_str());
    if (start_index + row == selected) {
      attroff(A_REVERSE);
    }
  }
}

void draw_branches(const std::vector<DemoBranch> &branches, int selected,
                   int start_row, int rows, int width, bool focused) {
  mvhline(start_row, 0, ' ', width);
  mvprintw(start_row, 2, "Branches");
  ++start_row;
  int start_index = 0;
  if (selected >= rows) {
    start_index = selected - rows + 1;
  }
  for (int i = 0; i < rows; ++i) {
    mvhline(start_row + i, 0, ' ', width);
  }
  for (int row = 0; row < rows &&
                   start_index + row < static_cast<int>(branches.size());
       ++row) {
    const auto &item = branches[start_index + row];
    if (start_index + row == selected) {
      attron(A_REVERSE | (focused ? A_BOLD : A_NORMAL));
    }
    mvprintw(start_row + row, 2, "%-22s %-20s %s", item.name.c_str(),
             item.last_commit.c_str(), item.note.c_str());
    if (start_index + row == selected) {
      attroff(A_REVERSE);
    }
  }
}

} // namespace

int run_demo_tui() {
  if (!isatty(fileno(stdout)) || !isatty(fileno(stdin)) || !isatty(fileno(stderr))) {
    spdlog::error("Demo TUI requires an interactive terminal");
    return 1;
  }
  if (initscr() == nullptr) {
    spdlog::error("Failed to initialize curses for demo TUI");
    return 1;
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
    init_pair(1, COLOR_CYAN, -1);
    init_pair(2, COLOR_GREEN, -1);
  }

  std::vector<DemoPullRequest> prs{
      {"octocat", "hello-world", 128,
       "Improve commit status notifications",
       "Ready to merge"},
      {"octocat", "hello-world", 129,
       "Refactor polling pipeline",
       "Awaiting checks"},
      {"spacecorp", "launchpad", 42,
       "Add branch protection overrides",
       "Review required"},
      {"spacecorp", "launchpad", 43,
       "Implement nightly workflow",
       "Draft"}};

  std::vector<DemoBranch> branches{{"feature/tui-refresh", "5be92c1",
                                    "Merged 2 hours ago"},
                                   {"feature/better-logs", "92c44fa",
                                    "Merged yesterday"},
                                   {"release/v1.5.0", "aa4f732",
                                    "Pending clean-up policy"},
                                   {"fix/issue-808", "bb12c08", "Stale 12 days"}};

  bool focus_prs = true;
  int pr_selection = 0;
  int branch_selection = 0;
  int height = LINES;
  int width = COLS;

  bool running = true;
  while (running) {
    height = LINES;
    width = COLS;
    erase();
    attron(COLOR_PAIR(1));
    draw_header(width);
    attroff(COLOR_PAIR(1));
    int available_rows = height - 4;
    int pr_rows = std::max(1, available_rows / 2);
    int branch_rows = std::max(1, available_rows - pr_rows - 1);
    int pr_start = 1;
    int branch_start = pr_start + pr_rows + 1;
    draw_pull_requests(prs, pr_selection, pr_start, pr_rows, width, focus_prs);
    draw_branches(branches, branch_selection, branch_start, branch_rows, width,
                  !focus_prs);
    draw_footer(height, width, focus_prs);
    refresh();

    int ch = getch();
    switch (ch) {
    case 'q':
    case 'Q':
      running = false;
      break;
    case KEY_UP:
      if (focus_prs) {
        if (pr_selection > 0) {
          --pr_selection;
        }
      } else {
        if (branch_selection > 0) {
          --branch_selection;
        }
      }
      break;
    case KEY_DOWN: {
      if (focus_prs) {
        int limit = static_cast<int>(prs.size());
        if (pr_selection + 1 < limit) {
          ++pr_selection;
        }
      } else {
        int limit = static_cast<int>(branches.size());
        if (branch_selection + 1 < limit) {
          ++branch_selection;
        }
      }
      break;
    }
    case '\t':
    case 't':
    case 'T':
      focus_prs = !focus_prs;
      break;
    default:
      break;
    }
  }

  endwin();
  return 0;
}

} // namespace agpm
