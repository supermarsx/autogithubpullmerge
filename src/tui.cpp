/**
 * @file tui.cpp
 * @brief Implementation of the terminal UI for autogithubpullmerge.
 *
 * Contains the implementation of the curses-based TUI for monitoring and
 * managing pull requests and branches interactively.
 */

#include "tui.hpp"
#include "log.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <thread>
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

std::string
format_duration_brief(std::chrono::steady_clock::duration duration) {
  if (duration <= std::chrono::steady_clock::duration::zero()) {
    return "0s";
  }
  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
  long total = seconds.count();
  if (total < 0)
    total = 0;
  long hours = total / 3600;
  long minutes = (total % 3600) / 60;
  long secs = total % 60;
  std::ostringstream oss;
  if (hours > 0) {
    oss << hours << 'h' << ' ';
  }
  if (minutes > 0 || hours > 0) {
    oss << minutes << 'm' << ' ';
  }
  oss << secs << 's';
  return oss.str();
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
      {"toggle_focus", "Switch Focus"},
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
      "toggle_focus", "quit",         "navigate_up", "navigate_down"};
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
  if (lower == "toggle_focus" || lower == "switch_focus" ||
      lower == "focus_toggle") {
    return "toggle_focus";
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
         bool log_sidecar, bool mcp_caddy_window, bool request_caddy_window)
    : client_(client), poller_(poller), log_limit_(log_limit),
      mcp_event_limit_(log_limit), log_sidecar_(log_sidecar),
      mcp_caddy_window_(mcp_caddy_window),
      request_caddy_window_(request_caddy_window) {
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
  if (branch_win_) {
    delwin(branch_win_);
    branch_win_ = nullptr;
  }
  if (log_win_) {
    delwin(log_win_);
    log_win_ = nullptr;
  }
  if (help_win_) {
    delwin(help_win_);
    help_win_ = nullptr;
  }
  if (mcp_win_) {
    delwin(mcp_win_);
    mcp_win_ = nullptr;
  }
  redraw_requested_.store(true, std::memory_order_relaxed);
}

void Tui::set_mcp_caddy(bool enabled) {
  if (mcp_caddy_window_ == enabled)
    return;
  mcp_caddy_window_ = enabled;
  last_h_ = 0;
  last_w_ = 0;
  if (!initialized_)
    return;
  if (mcp_win_) {
    delwin(mcp_win_);
    mcp_win_ = nullptr;
  }
  redraw_requested_.store(true, std::memory_order_relaxed);
}

void Tui::set_request_caddy(bool enabled) {
  if (request_caddy_window_ == enabled)
    return;
  request_caddy_window_ = enabled;
  last_h_ = 0;
  last_w_ = 0;
  if (!initialized_)
    return;
  if (request_win_) {
    delwin(request_win_);
    request_win_ = nullptr;
  }
  if (request_caddy_window_) {
    start_request_monitor();
  } else {
    stop_request_monitor();
  }
  redraw_requested_.store(true, std::memory_order_relaxed);
}

void Tui::add_mcp_event(const std::string &event) {
  std::lock_guard<std::mutex> lock(mcp_mutex_);
  mcp_events_.push_back(event);
  if (mcp_events_.size() > mcp_event_limit_) {
    auto trim = mcp_events_.size() - mcp_event_limit_;
    mcp_events_.erase(mcp_events_.begin(),
                      mcp_events_.begin() + static_cast<std::ptrdiff_t>(trim));
  }
  redraw_requested_.store(true, std::memory_order_relaxed);
}

void Tui::set_refresh_interval(std::chrono::milliseconds interval) {
  if (interval < std::chrono::milliseconds(100)) {
    interval = std::chrono::milliseconds(100);
  }
  refresh_interval_ = interval;
  redraw_requested_.store(true, std::memory_order_relaxed);
}

/**
 * Populate the default set of hotkey bindings.
 */
void Tui::initialize_default_hotkeys() {
  hotkey_help_order_ = {"refresh",      "merge",        "open",
                        "details",      "toggle_focus", "quit",
                        "navigate_up",  "navigate_down"};
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
#ifdef KEY_BTAB
  std::vector<HotkeyBinding> toggle_bindings{
      HotkeyBinding{static_cast<int>('\t'), "Tab"}};
  if (KEY_BTAB != '\t') {
    toggle_bindings.push_back(HotkeyBinding{KEY_BTAB, "Shift+Tab"});
  }
#else
  std::vector<HotkeyBinding> toggle_bindings{
      HotkeyBinding{static_cast<int>('\t'), "Tab"}};
#endif
  set_bindings_for_action("toggle_focus", toggle_bindings);
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
  poller_.set_stray_callback(
      [this](const std::vector<StrayBranch> &branches) {
        update_branches(branches);
      });
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
  if (request_caddy_window_) {
    start_request_monitor();
  }
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
  redraw_requested_.store(true, std::memory_order_relaxed);
}

void Tui::update_branches(const std::vector<StrayBranch> &branches) {
  branches_ = branches;
  if (branch_selected_ >= static_cast<int>(branches_.size())) {
    branch_selected_ =
        branches_.empty() ? 0 : static_cast<int>(branches_.size()) - 1;
  }
  redraw_requested_.store(true, std::memory_order_relaxed);
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
  redraw_requested_.store(true, std::memory_order_relaxed);
}

void Tui::start_request_monitor() {
  if (request_thread_running_.exchange(true))
    return;
  request_thread_ = std::thread([this] {
    while (request_thread_running_.load()) {
      Poller::RequestQueueSnapshot queue_snapshot =
          poller_.request_queue_snapshot();
      auto budget = poller_.rate_budget_snapshot();
      {
        std::lock_guard<std::mutex> lock(request_mutex_);
        request_snapshot_ = std::move(queue_snapshot);
        budget_snapshot_ = std::move(budget);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
  });
}

void Tui::stop_request_monitor() {
  if (!request_thread_running_.exchange(false))
    return;
  if (request_thread_.joinable()) {
    request_thread_.join();
  }
}

/**
 * Redraw the entire user interface based on current state.
 */
void Tui::draw() {
  if (!initialized_)
    return;
  if (branches_.empty()) {
    focus_branches_ = false;
    branch_selected_ = 0;
  }

  const bool color_capable = has_colors();
  int h = 0;
  int w = 0;
  getmaxyx(stdscr, h, w);

  int pr_height = 0;
  int pr_total_width = 0;
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
  int mcp_height = 0;
  int mcp_width = 0;
  int mcp_y = 0;
  int mcp_x = 0;
  int request_height = 0;
  int request_width = 0;
  int request_y = 0;
  int request_x = 0;

  if (log_sidecar_) {
    log_width = std::max(20, w / 3);
    if (log_width >= w) {
      log_width = std::max(1, w - 1);
    }
    pr_total_width = w - log_width;
    if (pr_total_width < 20) {
      pr_total_width = std::max(10, w / 2);
      log_width = std::max(1, w - pr_total_width);
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
    help_width = pr_total_width;
    log_y = 0;
    log_x = pr_total_width;
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
    pr_total_width = w;
    help_height = log_height;
    log_y = h - log_height;
    log_x = 0;
    help_y = h - help_height;
    help_x = w - help_width;
  }

  int caddy_count = (request_caddy_window_ ? 1 : 0) + (mcp_caddy_window_ ? 1 : 0);
  if (caddy_count > 0 && log_height > 0) {
    int available = log_height;
    int caddy_height = std::max(3, available / (caddy_count + 1));
    int total_caddy = caddy_height * caddy_count;
    if (total_caddy >= available) {
      total_caddy = std::max(0, available - 1);
      if (caddy_count > 0) {
        caddy_height = std::max(1, total_caddy / caddy_count);
      } else {
        caddy_height = 0;
      }
    }
    if (caddy_height <= 0 && caddy_count > 0) {
      caddy_height = 1;
    }
    log_height = std::max(1, available - total_caddy);
    int offset_y = log_y + log_height;
    if (request_caddy_window_) {
      request_height = std::min(caddy_height, std::max(0, available - (offset_y - log_y)));
      if (request_height <= 0 && total_caddy > 0) {
        request_height = std::min(total_caddy, std::max(1, available - log_height));
      }
      request_width = log_width;
      request_y = offset_y;
      request_x = log_x;
      offset_y += request_height;
    }
    if (mcp_caddy_window_) {
      mcp_height = std::min(caddy_height, std::max(0, available - (offset_y - log_y)));
      if (mcp_height <= 0 && total_caddy > 0) {
        mcp_height = std::min(total_caddy, std::max(1, available - (offset_y - log_y)));
      }
      mcp_width = log_width;
      mcp_y = offset_y;
      mcp_x = log_x;
    }
  }

  pr_height = std::max(1, pr_height);
  log_height = std::max(1, log_height);
  log_width = std::max(1, log_width);
  help_height = std::max(1, help_height);
  help_width = std::max(1, help_width);
  mcp_height = std::max(0, mcp_height);
  mcp_width = std::max(0, mcp_width);

  int branch_width = std::max(20, pr_total_width / 3);
  int pr_list_width = pr_total_width - branch_width;
  if (pr_list_width < 20) {
    pr_list_width = std::max(20, pr_total_width - 20);
    branch_width = pr_total_width - pr_list_width;
  }
  if (branch_width < 20) {
    branch_width = std::max(1, std::min(pr_total_width - 20, pr_total_width / 2));
    pr_list_width = pr_total_width - branch_width;
  }
  if (branch_width <= 0) {
    branch_width = 1;
    pr_list_width = std::max(1, pr_total_width - 1);
  }
  if (pr_list_width <= 0) {
    pr_list_width = 1;
    branch_width = std::max(1, pr_total_width - 1);
  }
  const int branch_x = pr_x + pr_list_width;
  const int branch_y = pr_y;
  const int branch_height = pr_height;

  if (!mcp_caddy_window_ && mcp_win_ != nullptr) {
    delwin(mcp_win_);
    mcp_win_ = nullptr;
  }
  if (!request_caddy_window_ && request_win_ != nullptr) {
    delwin(request_win_);
    request_win_ = nullptr;
  }

  if (h != last_h_ || w != last_w_ || pr_win_ == nullptr || branch_win_ == nullptr ||
      log_win_ == nullptr || help_win_ == nullptr ||
      (mcp_caddy_window_ && mcp_win_ == nullptr) ||
      (request_caddy_window_ && request_win_ == nullptr)) {
    last_h_ = h;
    last_w_ = w;
    if (pr_win_)
      delwin(pr_win_);
    if (branch_win_)
      delwin(branch_win_);
    if (log_win_)
      delwin(log_win_);
    if (help_win_)
      delwin(help_win_);
    if (mcp_win_) {
      delwin(mcp_win_);
      mcp_win_ = nullptr;
    }
    if (request_win_) {
      delwin(request_win_);
      request_win_ = nullptr;
    }
    if (detail_win_) {
      delwin(detail_win_);
      detail_win_ = nullptr;
    }
    pr_win_ = newwin(pr_height, pr_list_width, pr_y, pr_x);
    branch_win_ = newwin(branch_height, branch_width, branch_y, branch_x);
    log_win_ = newwin(log_height, log_width, log_y, log_x);
    help_win_ = newwin(help_height, help_width, help_y, help_x);
    if (request_caddy_window_ && request_height > 0 && request_width > 0) {
      request_win_ =
          newwin(std::max(1, request_height), std::max(1, request_width),
                 request_y, request_x);
    }
    if (mcp_caddy_window_ && mcp_height > 0 && mcp_width > 0) {
      mcp_win_ = newwin(std::max(1, mcp_height), std::max(1, mcp_width),
                        mcp_y, mcp_x);
    }
  }

  auto apply_background = [&](WINDOW *win) {
    if (!win)
      return;
    if (color_capable) {
      wbkgd(win, COLOR_PAIR(0));
      redrawwin(win);
    }
  };
  apply_background(pr_win_);
  apply_background(branch_win_);
  apply_background(log_win_);
  apply_background(help_win_);
  apply_background(request_win_);
  apply_background(mcp_win_);

  std::vector<std::string> mcp_snapshot;
  if (mcp_caddy_window_) {
    std::lock_guard<std::mutex> lock(mcp_mutex_);
    mcp_snapshot = mcp_events_;
  }
  Poller::RequestQueueSnapshot queue_snapshot;
  std::optional<GitHubPoller::RateBudgetSnapshot> budget_snapshot;
  if (request_caddy_window_) {
    std::lock_guard<std::mutex> lock(request_mutex_);
    queue_snapshot = request_snapshot_;
    budget_snapshot = budget_snapshot_;
  }

  auto begin_highlight = [&](WINDOW *win) {
    if (!win)
      return;
    if (color_capable) {
      wattron(win, COLOR_PAIR(1));
    } else {
      wattron(win, A_REVERSE);
    }
  };
  auto end_highlight = [&](WINDOW *win) {
    if (!win)
      return;
    if (color_capable) {
      wattroff(win, COLOR_PAIR(1));
    } else {
      wattroff(win, A_REVERSE);
    }
  };

  const bool focus_prs = !focus_branches_;
  // PR window
  werase(pr_win_);
  box(pr_win_, 0, 0);
  std::string pr_title = focus_prs ? "Active PRs *" : "Active PRs";
  mvwprintw(pr_win_, 0, 2, "%s", pr_title.c_str());
  int pr_win_h = 0;
  int pr_win_w = 0;
  getmaxyx(pr_win_, pr_win_h, pr_win_w);
  int max_pr_lines = std::max(0, pr_win_h - 2);
  for (int i = 0; i < static_cast<int>(prs_.size()) && i < max_pr_lines; ++i) {
    if (focus_prs && i == selected_)
      begin_highlight(pr_win_);
    mvwprintw(pr_win_, 1 + i, 1, "%s/%s #%d %s", prs_[i].owner.c_str(),
              prs_[i].repo.c_str(), prs_[i].number, prs_[i].title.c_str());
    if (focus_prs && i == selected_)
      end_highlight(pr_win_);
  }
  if (prs_.empty()) {
    mvwprintw(pr_win_, 1, 1, "No pull requests detected");
  }
  wnoutrefresh(pr_win_);

  // Branch window
  werase(branch_win_);
  box(branch_win_, 0, 0);
  std::string branch_title = focus_branches_ ? "Branches *" : "Branches";
  mvwprintw(branch_win_, 0, 2, "%s", branch_title.c_str());
  int branch_win_h = 0;
  int branch_win_w = 0;
  getmaxyx(branch_win_, branch_win_h, branch_win_w);
  int max_branch_lines = std::max(0, branch_win_h - 2);
  if (branches_.empty()) {
    mvwprintw(branch_win_, 1, 1, "No branches detected");
  } else {
    for (int i = 0; i < static_cast<int>(branches_.size()) &&
             i < max_branch_lines; ++i) {
      std::string line = branches_[i].owner + "/" + branches_[i].repo + " " +
                         branches_[i].name;
      if (branch_win_w > 2 && static_cast<int>(line.size()) > branch_win_w - 2) {
        if (branch_win_w > 5) {
          line = line.substr(0, branch_win_w - 5) + "...";
        } else {
          line = line.substr(0, std::max(0, branch_win_w - 2));
        }
      }
      if (focus_branches_ && i == branch_selected_)
        begin_highlight(branch_win_);
      mvwprintw(branch_win_, 1 + i, 1, "%s", line.c_str());
      if (focus_branches_ && i == branch_selected_)
        end_highlight(branch_win_);
    }
  }
  wnoutrefresh(branch_win_);

  // Log window
  werase(log_win_);
  box(log_win_, 0, 0);
  mvwprintw(log_win_, 0, 2, "Logs");
  int log_win_h = 0;
  int log_win_w = 0;
  getmaxyx(log_win_, log_win_h, log_win_w);
  int max_log_lines = std::max(0, log_win_h - 2);
  int start = logs_.size() > static_cast<std::size_t>(max_log_lines)
                  ? static_cast<int>(logs_.size()) - max_log_lines
                  : 0;
  for (int i = 0;
       start + i < static_cast<int>(logs_.size()) && i < max_log_lines; ++i) {
    if (color_capable)
      wattron(log_win_, COLOR_PAIR(2));
    mvwprintw(log_win_, 1 + i, 1, "%s", logs_[start + i].c_str());
    if (color_capable)
      wattroff(log_win_, COLOR_PAIR(2));
  }
  wnoutrefresh(log_win_);

  if (request_caddy_window_ && request_win_ != nullptr) {
    werase(request_win_);
    box(request_win_, 0, 0);
    mvwprintw(request_win_, 0, 2, "Request Queue");
    int req_win_h = 0;
    int req_win_w = 0;
    getmaxyx(request_win_, req_win_h, req_win_w);
    int row = 1;
    auto print_line = [&](std::string text) {
      if (row >= req_win_h - 1)
        return;
      if (req_win_w > 2 && static_cast<int>(text.size()) > req_win_w - 2) {
        if (req_win_w > 5) {
          text = text.substr(0, req_win_w - 5) + "...";
        } else {
          text = text.substr(0, std::max(0, req_win_w - 2));
        }
      }
      mvwprintw(request_win_, row++, 1, "%s", text.c_str());
    };
    auto now = std::chrono::steady_clock::now();
    std::string session_text = "Session --";
    if (queue_snapshot.session_start !=
        std::chrono::steady_clock::time_point{}) {
      session_text =
          "Session " + format_duration_brief(now - queue_snapshot.session_start);
    }
    std::size_t outstanding = queue_snapshot.pending.size() +
                              queue_snapshot.running.size();
    std::ostringstream stats_line;
    stats_line << session_text << " | Outstanding " << outstanding;
    stats_line << " | Done " << queue_snapshot.total_completed << "/"
               << queue_snapshot.total_failed;
    if (queue_snapshot.average_latency_ms) {
      stats_line << " | Avg " << std::fixed << std::setprecision(1)
                 << *queue_snapshot.average_latency_ms << "ms";
    } else {
      stats_line << " | Avg --";
    }
    if (queue_snapshot.clearance) {
      auto clearance_duration = std::chrono::duration_cast<
          std::chrono::steady_clock::duration>(*queue_snapshot.clearance);
      stats_line << " | Clearance " << format_duration_brief(clearance_duration);
    }
    print_line(stats_line.str());
    if (budget_snapshot) {
      std::ostringstream budget_line;
      budget_line << "Budget " << budget_snapshot->remaining << "/"
                  << budget_snapshot->limit;
      budget_line << " used " << budget_snapshot->used;
      budget_line << " reserve " << budget_snapshot->reserve;
      print_line(budget_line.str());
      if (!budget_snapshot->source.empty()) {
        budget_line.str(std::string{});
        budget_line.clear();
        budget_line << "Source " << budget_snapshot->source;
        print_line(budget_line.str());
      }
    }
    auto format_entry = [&](const Poller::RequestInfo &info) {
      std::ostringstream oss;
      oss << info.label;
      switch (info.state) {
      case Poller::RequestState::Pending:
        oss << " [pending]";
        break;
      case Poller::RequestState::Running:
        oss << " [running]";
        break;
      case Poller::RequestState::Completed:
        if (info.duration) {
          double ms =
              std::chrono::duration<double, std::milli>(*info.duration).count();
          oss << " [done " << std::fixed << std::setprecision(1) << ms << "ms]";
        } else {
          oss << " [done]";
        }
        break;
      case Poller::RequestState::Failed:
        oss << " [failed";
        if (!info.error.empty()) {
          oss << ": " << info.error;
        }
        oss << ']';
        break;
      case Poller::RequestState::Cancelled:
        oss << " [cancelled]";
        break;
      }
      return oss.str();
    };
    auto print_section = [&](const char *title,
                             const std::vector<Poller::RequestInfo> &items,
                             bool reverse = false) {
      if (items.empty())
        return;
      print_line(title);
      if (row >= req_win_h - 1)
        return;
      if (reverse) {
        for (auto it = items.rbegin();
             it != items.rend() && row < req_win_h - 1; ++it) {
          print_line(format_entry(*it));
        }
      } else {
        for (const auto &item : items) {
          if (row >= req_win_h - 1)
            break;
          print_line(format_entry(item));
        }
      }
    };
    print_section("Active:", queue_snapshot.running);
    print_section("Pending:", queue_snapshot.pending);
    print_section("Done:", queue_snapshot.completed, true);
    wnoutrefresh(request_win_);
  }

  if (mcp_caddy_window_ && mcp_win_ != nullptr) {
    werase(mcp_win_);
    box(mcp_win_, 0, 0);
    mvwprintw(mcp_win_, 0, 2, "MCP Activity");
    int mcp_win_h = 0;
    int mcp_win_w = 0;
    getmaxyx(mcp_win_, mcp_win_h, mcp_win_w);
    int max_mcp_lines = std::max(0, mcp_win_h - 2);
    int start_index = static_cast<int>(mcp_snapshot.size()) - max_mcp_lines;
    if (start_index < 0)
      start_index = 0;
    for (int i = 0; i < max_mcp_lines &&
                    start_index + i < static_cast<int>(mcp_snapshot.size()); ++i) {
      std::string entry = mcp_snapshot[static_cast<std::size_t>(start_index + i)];
      if (mcp_win_w > 2 && static_cast<int>(entry.size()) > mcp_win_w - 2) {
        if (mcp_win_w > 5) {
          entry = entry.substr(0, mcp_win_w - 5) + "...";
        } else {
          entry = entry.substr(0, std::max(0, mcp_win_w - 2));
        }
      }
      mvwprintw(mcp_win_, 1 + i, 1, "%s", entry.c_str());
    }
    wnoutrefresh(mcp_win_);
  }

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
  if (color_capable)
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
  if (color_capable)
    wattroff(help_win_, COLOR_PAIR(3));
  wnoutrefresh(help_win_);

  if (detail_visible_) {
    const int dh = h / 2;
    if (!detail_win_) {
      const int dw = w / 2;
      detail_win_ = newwin(dh, dw, (h - dh) / 2, (w - dw) / 2);
      apply_background(detail_win_);
    }
    werase(detail_win_);
    box(detail_win_, 0, 0);
    mvwprintw(detail_win_, 0, 2, "PR Details");
    if (!prs_.empty() && selected_ < static_cast<int>(prs_.size())) {
      const auto &pr = prs_[selected_];
      mvwprintw(detail_win_, 1, 1, "%s/%s #%d", pr.owner.c_str(), pr.repo.c_str(),
                pr.number);
    }
    mvwprintw(detail_win_, 2, 1, "%s", detail_text_.c_str());
    mvwprintw(detail_win_, dh - 2, 1, "Press ENTER or d to close");
    wnoutrefresh(detail_win_);
  } else if (detail_win_) {
    delwin(detail_win_);
    detail_win_ = nullptr;
  }

  doupdate();
  redraw_requested_.store(false, std::memory_order_relaxed);
}
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
  auto request_redraw = [&]() {
    redraw_requested_.store(true, std::memory_order_relaxed);
  };
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
        request_redraw();
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
    request_redraw();
  } else if (action == "navigate_up") {
    if (focus_branches_ && !branches_.empty()) {
      if (branch_selected_ > 0) {
        --branch_selected_;
        tui_log()->debug("Moved branch selection up to {}", branch_selected_);
        request_redraw();
      }
    } else if (selected_ > 0) {
      --selected_;
      tui_log()->debug("Moved selection up to {}", selected_);
      request_redraw();
    }
  } else if (action == "navigate_down") {
    if (focus_branches_ && !branches_.empty()) {
      if (branch_selected_ + 1 < static_cast<int>(branches_.size())) {
        ++branch_selected_;
        tui_log()->debug("Moved branch selection down to {}", branch_selected_);
        request_redraw();
      }
    } else if (selected_ + 1 < static_cast<int>(prs_.size())) {
      ++selected_;
      tui_log()->debug("Moved selection down to {}", selected_);
      request_redraw();
    }
  } else if (action == "toggle_focus") {
    if (!branches_.empty()) {
      focus_branches_ = !focus_branches_;
      tui_log()->debug("Focus switched to {}", focus_branches_ ? "branches"
                                                               : "pull requests");
      request_redraw();
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
  redraw_requested_.store(true, std::memory_order_relaxed);
  auto next_refresh =
      std::chrono::steady_clock::now() + refresh_interval_;
  while (running_) {
    auto now = std::chrono::steady_clock::now();
    bool due = now >= next_refresh;
    bool requested =
        redraw_requested_.exchange(false, std::memory_order_relaxed);
    if (due || requested) {
      draw();
      next_refresh = std::chrono::steady_clock::now() + refresh_interval_;
      continue;
    }
    auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        next_refresh - std::chrono::steady_clock::now());
    long wait_ms = remaining.count();
    if (wait_ms < 0)
      wait_ms = 0;
    const long min_wait = 10;
    const long max_wait = 1000;
    long timeout_ms = wait_ms == 0 ? 0
                                   : std::clamp(wait_ms, min_wait, max_wait);
    timeout(static_cast<int>(timeout_ms));
    int ch = getch();
    if (ch == ERR) {
      if (timeout_ms == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(min_wait));
      }
      continue;
    }
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
  poller_.set_stray_callback(nullptr);
  stop_request_monitor();
  if (pr_win_) {
    delwin(pr_win_);
    pr_win_ = nullptr;
  }
  if (branch_win_) {
    delwin(branch_win_);
    branch_win_ = nullptr;
  }
  if (log_win_)
    delwin(log_win_);
  if (help_win_)
    delwin(help_win_);
  if (mcp_win_)
    delwin(mcp_win_);
  if (detail_win_)
    delwin(detail_win_);
  endwin();
  initialized_ = false;
}

} // namespace agpm
