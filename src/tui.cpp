#include "tui.hpp"
#include "log.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#if defined(_WIN32)
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

namespace {

std::shared_ptr<spdlog::logger> tui_log() {
  static auto logger = [] {
    agpm::ensure_default_logger();
    return agpm::category_logger("tui");
  }();
  return logger;
}

struct ParsedBinding {
  int key;
  std::string label;
};

/**
 * Return a trimmed copy of the input string.
 *
 * @param s String potentially containing leading or trailing whitespace.
 * @return New string without surrounding whitespace.
 */
std::string trim_copy(const std::string &s) {
  auto begin = std::find_if_not(s.begin(), s.end(), [](unsigned char ch) {
    return static_cast<bool>(std::isspace(ch));
  });
  auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char ch) {
               return static_cast<bool>(std::isspace(ch));
             }).base();
  if (begin >= end) {
    return {};
  }
  return std::string(begin, end);
}

/**
 * Produce a lowercase copy of the string.
 *
 * @param s Input string to normalize.
 * @return Lowercase variant of the input string.
 */
std::string to_lower_copy(const std::string &s) {
  std::string result;
  result.reserve(s.size());
  std::transform(s.begin(), s.end(), std::back_inserter(result),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return result;
}

/**
 * Split a binding list specification into individual tokens.
 *
 * @param spec Comma- or pipe-separated list of bindings.
 * @return Vector of trimmed binding strings.
 */
std::vector<std::string> split_binding_list(const std::string &spec) {
  std::vector<std::string> parts;
  std::string current;
  for (char ch : spec) {
    if (ch == ',' || ch == '|') {
      std::string trimmed = trim_copy(current);
      if (!trimmed.empty()) {
        parts.push_back(trimmed);
      }
      current.clear();
    } else {
      current.push_back(ch);
    }
  }
  std::string trimmed = trim_copy(current);
  if (!trimmed.empty()) {
    parts.push_back(trimmed);
  }
  return parts;
}

/**
 * Parse a textual binding specification into key codes and labels.
 *
 * @param spec Binding description string such as `"ctrl+c"`.
 * @return Parsed key codes paired with human-friendly labels.
 */
std::vector<ParsedBinding> parse_binding_spec(const std::string &spec) {
  std::vector<ParsedBinding> result;
  if (spec.empty()) {
    return result;
  }
  std::string lower = to_lower_copy(spec);
  if (lower == "\\n" || lower == "enter" || lower == "return" ||
      lower == "newline" || lower == "key_enter") {
#ifdef KEY_ENTER
    result.push_back({KEY_ENTER, "Enter"});
#endif
    result.push_back({static_cast<int>('\n'), "Enter"});
    return result;
  }
  if (lower == "space" || lower == "spacebar") {
    result.push_back({static_cast<int>(' '), "Space"});
    return result;
  }
  if (lower == "\\t" || lower == "tab") {
    result.push_back({static_cast<int>('\t'), "Tab"});
    return result;
  }
  if (lower == "escape" || lower == "esc") {
    result.push_back({27, "Escape"});
    return result;
  }
  if (lower == "up" || lower == "arrow_up" || lower == "key_up" ||
      lower == "cursor_up") {
#ifdef KEY_UP
    result.push_back({KEY_UP, "Up Arrow"});
#endif
    return result;
  }
  if (lower == "down" || lower == "arrow_down" || lower == "key_down" ||
      lower == "cursor_down") {
#ifdef KEY_DOWN
    result.push_back({KEY_DOWN, "Down Arrow"});
#endif
    return result;
  }
  if (lower.rfind("ctrl+", 0) == 0) {
    std::string suffix = trim_copy(spec.substr(5));
    if (suffix.size() == 1) {
      unsigned char ch = static_cast<unsigned char>(suffix[0]);
      unsigned char upper = static_cast<unsigned char>(std::toupper(ch));
      int key = static_cast<int>(upper & 0x1F);
      std::string label = "Ctrl+";
      label.push_back(static_cast<char>(upper));
      result.push_back({key, label});
    }
    return result;
  }
  if (lower.size() == 1) {
    unsigned char ch = static_cast<unsigned char>(spec[0]);
    std::string label(1, static_cast<char>(spec[0]));
    result.push_back({static_cast<int>(ch), label});
    return result;
  }
  return result;
}

/**
 * Retrieve human-readable descriptions for hotkey actions.
 *
 * @return Reference to a static mapping from action identifiers to labels.
 */
const std::unordered_map<std::string, std::string> &action_descriptions() {
  static const std::unordered_map<std::string, std::string> descriptions{
      {"refresh", "Refresh"},
      {"merge", "Merge"},
      {"open", "Open PR"},
      {"details", "Toggle Details"},
      {"quit", "Quit"},
      {"navigate_up", "Select Previous"},
      {"navigate_down", "Select Next"}};
  return descriptions;
}

/**
 * Retrieve the set of valid action identifiers.
 *
 * @return Reference to the static set of canonical action names.
 */
const std::unordered_set<std::string> &valid_actions() {
  static const std::unordered_set<std::string> actions{
      "refresh",      "merge",        "open",        "details",
      "quit",         "navigate_up",  "navigate_down"};
  return actions;
}

/**
 * Normalize action names to canonical identifiers.
 *
 * @param action Input action string provided by configuration.
 * @return Canonical action identifier recognized by the TUI.
 */
std::string canonicalize_action(const std::string &action) {
  std::string lower = to_lower_copy(action);
  if (lower == "open_pr" || lower == "open-pr" || lower == "openpr") {
    return "open";
  }
  if (lower == "detail" || lower == "toggle_detail" ||
      lower == "toggle_details" || lower == "toggle-details") {
    return "details";
  }
  if (lower == "up" || lower == "previous" || lower == "prev") {
    return "navigate_up";
  }
  if (lower == "down" || lower == "next") {
    return "navigate_down";
  }
  return lower;
}

} // namespace

namespace agpm {

/**
 * Construct the text UI and configure default hotkeys.
 *
 * @param client GitHub client used for interactive operations.
 * @param poller Poller providing live data updates.
 * @param log_limit Maximum number of log entries to retain.
 */
Tui::Tui(GitHubClient &client, GitHubPoller &poller, std::size_t log_limit,
         bool log_sidecar)
    : client_(client), poller_(poller), log_limit_(log_limit),
      log_sidecar_(log_sidecar) {
  ensure_default_logger();
  initialize_default_hotkeys();
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
}

void Tui::set_log_sidecar(bool enabled) {
  if (log_sidecar_ == enabled)
    return;
  log_sidecar_ = enabled;
  last_h_ = 0;
  last_w_ = 0;
  if (!initialized_)
    return;
  if (pr_win_) {
    delwin(pr_win_);
    pr_win_ = nullptr;
  }
  if (log_win_) {
    delwin(log_win_);
    log_win_ = nullptr;
  }
  if (help_win_) {
    delwin(help_win_);
    help_win_ = nullptr;
  }
}

/**
 * Populate the default set of hotkey bindings.
 */
void Tui::initialize_default_hotkeys() {
  hotkey_help_order_ = {"refresh",      "merge",        "open",
                        "details",      "quit",         "navigate_up",
                        "navigate_down"};
  action_bindings_.clear();
  key_to_action_.clear();
  set_bindings_for_action("refresh",
                          {HotkeyBinding{static_cast<int>('r'), "r"}});
  set_bindings_for_action("merge",
                          {HotkeyBinding{static_cast<int>('m'), "m"}});
  set_bindings_for_action("open",
                          {HotkeyBinding{static_cast<int>('o'), "o"}});
  std::vector<HotkeyBinding> detail_bindings;
  detail_bindings.push_back(
      HotkeyBinding{static_cast<int>('d'), std::string("d")});
#ifdef KEY_ENTER
  if (KEY_ENTER != '\n') {
    detail_bindings.push_back(HotkeyBinding{KEY_ENTER, "Enter"});
  }
#endif
  detail_bindings.push_back(
      HotkeyBinding{static_cast<int>('\n'), std::string("Enter")});
  set_bindings_for_action("details", detail_bindings);
  set_bindings_for_action("quit",
                          {HotkeyBinding{static_cast<int>('q'), "q"}});
#ifdef KEY_UP
  set_bindings_for_action("navigate_up", {HotkeyBinding{KEY_UP, "Up Arrow"}});
#else
  set_bindings_for_action("navigate_up",
                          {HotkeyBinding{static_cast<int>('k'), "k"}});
#endif
#ifdef KEY_DOWN
  set_bindings_for_action("navigate_down",
                          {HotkeyBinding{KEY_DOWN, "Down Arrow"}});
#else
  set_bindings_for_action("navigate_down",
                          {HotkeyBinding{static_cast<int>('j'), "j"}});
#endif
}

/**
 * Remove all bindings currently assigned to an action.
 *
 * @param action Canonical action identifier whose bindings should be cleared.
 */
void Tui::clear_action_bindings(const std::string &action) {
  auto it = action_bindings_.find(action);
  if (it == action_bindings_.end()) {
    action_bindings_.emplace(action, std::vector<HotkeyBinding>{});
    return;
  }
  for (const auto &binding : it->second) {
    auto key_it = key_to_action_.find(binding.key);
    if (key_it != key_to_action_.end() && key_it->second == action) {
      key_to_action_.erase(key_it);
    }
  }
  it->second.clear();
}

/**
 * Replace the bindings associated with an action.
 *
 * @param action Canonical action identifier receiving new bindings.
 * @param bindings Hotkey bindings to assign to the action.
 */
void Tui::set_bindings_for_action(
    const std::string &action, const std::vector<HotkeyBinding> &bindings) {
  clear_action_bindings(action);
  auto &vec = action_bindings_[action];
  vec.reserve(bindings.size());
  std::unordered_set<int> seen;
  for (const auto &binding : bindings) {
    if (!seen.insert(binding.key).second) {
      continue;
    }
    auto existing = key_to_action_.find(binding.key);
    if (existing != key_to_action_.end() && existing->second != action) {
      tui_log()->warn(
          "Hotkey '{}' already assigned to '{}'; '{}' will take precedence",
          binding.label, existing->second, action);
    }
    key_to_action_[binding.key] = action;
    vec.push_back(binding);
  }
}

/**
 * Apply user-provided hotkey configuration overrides.
 *
 * @param bindings Mapping from action identifiers to configuration strings.
 */
void Tui::configure_hotkeys(
    const std::unordered_map<std::string, std::string> &bindings) {
  for (const auto &entry : bindings) {
    std::string canonical = canonicalize_action(entry.first);
    if (valid_actions().count(canonical) == 0) {
      tui_log()->warn("Unknown hotkey action '{}' in configuration", entry.first);
      continue;
    }
    std::string value = trim_copy(entry.second);
    if (value.empty()) {
      tui_log()->info("Disabling hotkey bindings for action '{}'", canonical);
      set_bindings_for_action(canonical, {});
      continue;
    }
    std::string lower_value = to_lower_copy(value);
    if (lower_value == "default") {
      continue;
    }
    if (lower_value == "none" || lower_value == "disabled" ||
        lower_value == "off") {
      tui_log()->info("Disabling hotkey bindings for action '{}'", canonical);
      set_bindings_for_action(canonical, {});
      continue;
    }
    std::vector<std::string> specs = split_binding_list(value);
    std::vector<HotkeyBinding> parsed;
    for (const auto &spec : specs) {
      std::vector<ParsedBinding> parsed_list = parse_binding_spec(spec);
      if (parsed_list.empty()) {
        tui_log()->warn(
            "Ignoring unrecognized hotkey binding '{}' for action '{}'", spec,
            canonical);
        continue;
      }
      for (const auto &p : parsed_list) {
        parsed.push_back(HotkeyBinding{p.key, p.label});
      }
    }
    if (parsed.empty()) {
      tui_log()->warn(
          "No valid hotkey bindings provided for action '{}'; keeping existing "
          "bindings",
          canonical);
      continue;
    }
    set_bindings_for_action(canonical, parsed);
  }
}

/**
 * Destructor ensures curses resources are released.
 */
Tui::~Tui() {
  // Ensure curses resources are released if init() succeeded
  try {
    cleanup();
  } catch (...) {
    // Never throw in a destructor
  }
}

/**
 * Initialize the curses environment and install callbacks.
 *
 * @throws std::runtime_error When the curses subsystem cannot be initialized.
 */
void Tui::init() {
  // Ensure all standard streams are attached to a real terminal. macOS
  // builds in CI environments may have one or more redirected which can
  // cause ncurses to crash with a bus error when initializing.
  bool tty_out = isatty(fileno(stdout));
  bool tty_in = isatty(fileno(stdin));
  bool tty_err = isatty(fileno(stderr));
  if (!tty_out || !tty_in || !tty_err) {
    return;
  }
  if (initscr() == nullptr) {
    throw std::runtime_error("Failed to initialize curses");
  }
  // Attach callbacks only when UI is truly active
  poller_.set_pr_callback(
      [this](const std::vector<PullRequest> &prs) { update_prs(prs); });
  poller_.set_log_callback([this](const std::string &msg) { log(msg); });
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

/**
 * Update the set of pull requests displayed by the UI.
 *
 * @param prs Latest pull request list to render.
 */
void Tui::update_prs(const std::vector<PullRequest> &prs) {
  prs_ = prs;
  if (selected_ >= static_cast<int>(prs_.size())) {
    selected_ = prs_.empty() ? 0 : static_cast<int>(prs_.size()) - 1;
  }
}

/**
 * Append a message to the in-memory log buffer.
 *
 * @param msg Log message to store.
 */
void Tui::log(const std::string &msg) {
  logs_.push_back(msg);
  if (logs_.size() > log_limit_) {
    logs_.erase(logs_.begin(), logs_.begin() + (logs_.size() - log_limit_));
  }
}

/**
 * Redraw the entire user interface based on current state.
 */
void Tui::draw() {
  if (!initialized_)
    return;
  int h, w;
  getmaxyx(stdscr, h, w);
  int pr_height = 0;
  int pr_width = 0;
  const int pr_y = 0;
  const int pr_x = 0;
  int log_height = 0;
  int log_width = 0;
  int log_y = 0;
  int log_x = 0;
  int help_height = 0;
  int help_width = 0;
  int help_y = 0;
  int help_x = 0;

  if (log_sidecar_) {
    log_width = std::max(20, w / 3);
    if (log_width >= w) {
      log_width = std::max(1, w - 1);
    }
    pr_width = w - log_width;
    if (pr_width < 20) {
      pr_width = std::max(10, w / 2);
      log_width = std::max(1, w - pr_width);
    }
    log_height = std::max(3, h);
    help_height = std::max(3, h / 4);
    if (help_height >= h) {
      help_height = std::max(3, h - 3);
    }
    pr_height = h - help_height;
    if (pr_height < 3) {
      pr_height = std::max(3, h - 3);
      help_height = h - pr_height;
    }
    help_width = pr_width;
    log_y = 0;
    log_x = pr_width;
    help_y = pr_height;
    help_x = 0;
  } else {
    log_height = std::max(3, h / 3);
    if (log_height >= h) {
      log_height = std::max(3, h - 3);
    }
    help_width = std::max(20, w / 3);
    if (help_width >= w) {
      help_width = std::max(20, w - 1);
    }
    log_width = w - help_width;
    if (log_width < 10) {
      log_width = std::max(10, w - 10);
      help_width = w - log_width;
    }
    pr_height = h - log_height;
    if (pr_height < 3) {
      pr_height = std::max(3, h - 3);
      log_height = h - pr_height;
    }
    pr_width = w;
    help_height = log_height;
    log_y = h - log_height;
    log_x = 0;
    help_y = h - help_height;
    help_x = w - help_width;
  }

  pr_height = std::max(1, pr_height);
  pr_width = std::max(1, pr_width);
  log_height = std::max(1, log_height);
  log_width = std::max(1, log_width);
  help_height = std::max(1, help_height);
  help_width = std::max(1, help_width);

  if (h != last_h_ || w != last_w_ || pr_win_ == nullptr ||
      log_win_ == nullptr || help_win_ == nullptr) {
    last_h_ = h;
    last_w_ = w;
    if (pr_win_)
      delwin(pr_win_);
    if (log_win_)
      delwin(log_win_);
    if (help_win_)
      delwin(help_win_);
    if (detail_win_) {
      delwin(detail_win_);
      detail_win_ = nullptr;
    }
    pr_win_ = newwin(pr_height, pr_width, pr_y, pr_x);
    log_win_ = newwin(log_height, log_width, log_y, log_x);
    help_win_ = newwin(help_height, help_width, help_y, help_x);
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
  int pr_win_h = 0;
  int pr_win_w = 0;
  getmaxyx(pr_win_, pr_win_h, pr_win_w);
  int max_pr_lines = std::max(0, pr_win_h - 2);
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
  int log_win_h = 0;
  int log_win_w = 0;
  getmaxyx(log_win_, log_win_h, log_win_w);
  int max_log_lines = std::max(0, log_win_h - 2);
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
  const auto &descriptions = action_descriptions();
  int line = 1;
  int help_win_h = 0;
  int help_win_w = 0;
  getmaxyx(help_win_, help_win_h, help_win_w);
  int max_lines = std::max(0, help_win_h - 2);
  if (has_colors())
    wattron(help_win_, COLOR_PAIR(3));
  for (const auto &action : hotkey_help_order_) {
    if (line > max_lines)
      break;
    auto desc_it = descriptions.find(action);
    if (desc_it == descriptions.end())
      continue;
    std::string keys_text;
    auto binding_it = action_bindings_.find(action);
    if (binding_it != action_bindings_.end() && !binding_it->second.empty()) {
      const auto &bindings = binding_it->second;
      for (std::size_t i = 0; i < bindings.size(); ++i) {
        if (i > 0)
          keys_text += ", ";
        keys_text += bindings[i].label;
      }
    } else {
      keys_text = "disabled";
    }
    mvwprintw(help_win_, line++, 1, "%s - %s", keys_text.c_str(),
              desc_it->second.c_str());
  }
  if (has_colors())
    wattroff(help_win_, COLOR_PAIR(3));
  wrefresh(help_win_);

  if (detail_visible_) {
    const int dh = h / 2;
    if (!detail_win_) {
      const int dw = w / 2;
      detail_win_ = newwin(dh, dw, (h - dh) / 2, (w - dw) / 2);
    }
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

/**
 * Process a single key press event.
 *
 * @param ch Character code received from `getch()`.
 */
void Tui::handle_key(int ch) {
  if (!initialized_)
    return;
  tui_log()->debug("Key pressed: {}", ch);
  auto it = key_to_action_.find(ch);
  if (it == key_to_action_.end()) {
    tui_log()->debug("Unhandled key: {}", ch);
    return;
  }
  const std::string &action = it->second;
  const bool is_navigation =
      (action == "navigate_up" || action == "navigate_down");
  const bool is_quit = (action == "quit");
  if (!hotkeys_enabled_ && !is_navigation && !is_quit) {
    tui_log()->debug("Hotkeys disabled; ignoring action {}", action);
    return;
  }
  if (action == "quit") {
    tui_log()->info("Quit requested");
    running_ = false;
  } else if (action == "refresh") {
    tui_log()->info("Manual refresh requested");
    poller_.poll_now();
  } else if (action == "merge") {
    if (selected_ < static_cast<int>(prs_.size())) {
      const auto &pr = prs_[selected_];
      tui_log()->info("Merge requested for PR #{}", pr.number);
      if (client_.merge_pull_request(pr.owner, pr.repo, pr.number)) {
        log("Merged PR #" + std::to_string(pr.number));
        prs_.erase(prs_.begin() + selected_);
        if (selected_ >= static_cast<int>(prs_.size())) {
          selected_ = prs_.empty() ? 0 : static_cast<int>(prs_.size()) - 1;
        }
        if (detail_visible_) {
          detail_visible_ = false;
          detail_text_.clear();
        }
      }
    }
  } else if (action == "open") {
    if (selected_ < static_cast<int>(prs_.size())) {
      const auto &pr = prs_[selected_];
      std::string url = "https://github.com/" + pr.owner + "/" + pr.repo +
                        "/pull/" + std::to_string(pr.number);
      open_cmd_(url);
    }
  } else if (action == "details") {
    if (detail_visible_) {
      detail_visible_ = false;
    } else if (selected_ < static_cast<int>(prs_.size())) {
      const auto &pr = prs_[selected_];
      detail_text_ = pr.title;
      detail_visible_ = true;
    }
  } else if (action == "navigate_up") {
    if (selected_ > 0) {
      --selected_;
      tui_log()->debug("Moved selection up to {}", selected_);
    }
  } else if (action == "navigate_down") {
    if (selected_ + 1 < static_cast<int>(prs_.size())) {
      ++selected_;
      tui_log()->debug("Moved selection down to {}", selected_);
    }
  } else {
    tui_log()->debug("Unhandled action '{}' for key {}", action, ch);
  }
}

/**
 * Enter the main UI loop, processing input until exit.
 */
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

/**
 * Restore terminal state after the UI terminates.
 */
void Tui::cleanup() {
  if (!initialized_)
    return;
  // Detach callbacks to avoid dangling references during teardown
  poller_.set_pr_callback(nullptr);
  poller_.set_log_callback(nullptr);
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
