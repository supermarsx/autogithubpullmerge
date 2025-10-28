#include "cli.hpp"
#include "curl/curl.h"
#include "log.hpp"
#include "token_loader.hpp"
#include "util/duration.hpp"
#include "version.hpp"
#include <CLI/CLI.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unordered_set>

#include <spdlog/spdlog.h>

namespace agpm {

namespace {
std::shared_ptr<spdlog::logger> cli_log() {
  static auto logger = [] {
    ensure_default_logger();
    return category_logger("cli");
  }();
  return logger;
}

std::string log_category_help_text() {
  static const std::array<std::string_view, 12> categories = {
      "app",           "cli",           "config",         "demo_tui",
      "github.client", "github.poller", "history",        "logging",
      "main",          "pat",           "repo.discovery", "tui"};
  std::ostringstream oss;
  oss << "Logging categories: ";
  for (std::size_t i = 0; i < categories.size(); ++i) {
    if (i != 0) {
      oss << ", ";
    }
    oss << categories[i];
  }
  oss << "\nUse --log-category NAME=LEVEL to override (e.g., "
         "repo.discovery=debug).";
  oss << " Configuration files accept the same mapping under 'log_categories'.";
  return oss.str();
}
} // namespace

/**
 * libcurl write callback that appends the received data to a string buffer.
 *
 * @param contents Pointer to the incoming data chunk.
 * @param size Size of each data element in bytes.
 * @param nmemb Number of elements provided.
 * @param userp Pointer to the destination std::string.
 * @return Number of bytes processed, signalling success to libcurl.
 */
static size_t write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t total = size * nmemb;
  std::string *s = static_cast<std::string *>(userp);
  s->append(static_cast<char *>(contents), total);
  return total;
}

/**
 * Build a descriptive error message for a CURL operation.
 *
 * @param verb HTTP verb attempted (e.g., "GET").
 * @param url Target URL of the request.
 * @param code CURL error code returned by the operation.
 * @param errbuf Optional error buffer filled by CURL with more detail.
 * @return Human-readable description summarizing the failure.
 */
static std::string format_curl_error(const char *verb, const std::string &url,
                                     CURLcode code, const char *errbuf) {
  std::ostringstream oss;
  oss << "curl " << verb;
  if (!url.empty()) {
    oss << ' ' << url;
  }
  oss << " failed: " << curl_easy_strerror(code);
  if (errbuf != nullptr && errbuf[0] != '\0') {
    oss << " - " << errbuf;
  }
  if (code == CURLE_OPERATION_TIMEDOUT) {
    oss << " - " << curl_easy_strerror(CURLE_COULDNT_CONNECT);
  }
  return oss.str();
}

/**
 * Retrieve newline-delimited personal access tokens from an HTTP endpoint.
 *
 * @param url Remote location containing tokens, one per line.
 * @param user Optional basic authentication user name.
 * @param pass Optional basic authentication password.
 * @return Collection of tokens extracted from the remote resource.
 */
static std::vector<std::string> load_tokens_from_url(const std::string &url,
                                                     const std::string &user,
                                                     const std::string &pass) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    throw std::runtime_error("Failed to init curl");
  }
  std::string response;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  if (!user.empty()) {
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(curl, CURLOPT_USERNAME, user.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, pass.c_str());
  }
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  char errbuf[CURL_ERROR_SIZE];
  errbuf[0] = '\0';
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  CURLcode res = curl_easy_perform(curl);
  std::string errstr;
  if (res != CURLE_OK) {
    errstr = format_curl_error("GET", url, res, errbuf);
  }
  curl_easy_cleanup(curl);
  if (res != CURLE_OK) {
    throw std::runtime_error(errstr);
  }
  std::vector<std::string> tokens;
  std::stringstream ss(response);
  std::string line;
  while (std::getline(ss, line)) {
    if (!line.empty()) {
      tokens.push_back(line);
    }
  }
  return tokens;
}

/**
 * Fetch an environment variable in a cross-platform, secure manner.
 *
 * @param name Null-terminated environment variable name.
 * @return Variable contents or an empty string if unavailable.
 */
static std::string get_env_var(const char *name) {
#ifdef _WIN32
  char *buf = nullptr;
  size_t sz = 0;
  if (_dupenv_s(&buf, &sz, name) == 0 && buf) {
    std::string value(buf);
    std::free(buf);
    return value;
  }
  return {};
#else
  const char *env = std::getenv(name);
  return env ? std::string(env) : std::string();
#endif
}

/**
 * Attempt to canonicalize a filesystem path while tolerating missing segments.
 *
 * @param input Path provided by the user or discovered during scanning.
 * @return Canonical path when available, otherwise a lexically-normalized path.
 */
static std::filesystem::path
canonicalize_path(const std::filesystem::path &input) {
  if (input.empty()) {
    return {};
  }
  std::error_code ec;
  auto canonical = std::filesystem::weakly_canonical(input, ec);
  if (!ec) {
    return canonical;
  }
  ec.clear();
  canonical = std::filesystem::absolute(input, ec);
  if (!ec) {
    return canonical.lexically_normal();
  }
  return input.lexically_normal();
}

/**
 * Discover token files stored in common directories on the host system.
 *
 * @param exe_path Path to the running executable (argv[0]).
 * @param repo_roots Directories supplied via repository discovery options.
 * @param existing_files Canonical paths already provided explicitly by the
 * user.
 * @return Collection of unique token file paths.
 */
static std::vector<std::filesystem::path>
discover_token_files(const std::filesystem::path &exe_path,
                     const std::vector<std::string> &repo_roots,
                     const std::unordered_set<std::string> &existing_files) {
  std::vector<std::filesystem::path> search_dirs;
  search_dirs.reserve(16);
  std::unordered_set<std::string> seen_dirs;
  auto add_dir = [&](const std::filesystem::path &dir) {
    if (dir.empty()) {
      return;
    }
    auto canonical_dir = canonicalize_path(dir);
    if (canonical_dir.empty()) {
      return;
    }
    std::error_code ec;
    if (!std::filesystem::exists(canonical_dir, ec) ||
        !std::filesystem::is_directory(canonical_dir, ec)) {
      return;
    }
    auto key = canonical_dir.generic_string();
    if (key.empty()) {
      return;
    }
    if (seen_dirs.insert(key).second) {
      search_dirs.push_back(canonical_dir);
    }
  };

  if (!exe_path.empty()) {
    auto exe_full = canonicalize_path(exe_path);
    std::filesystem::path exe_dir =
        exe_full.has_parent_path() ? exe_full.parent_path() : exe_full;
    add_dir(exe_dir);
  }

  add_dir(std::filesystem::current_path());

  for (const auto &root : repo_roots) {
    add_dir(root);
  }

  auto home_env = get_env_var("HOME");
#ifdef _WIN32
  if (home_env.empty()) {
    home_env = get_env_var("USERPROFILE");
  }
  if (home_env.empty()) {
    auto drive = get_env_var("HOMEDRIVE");
    auto path = get_env_var("HOMEPATH");
    if (!drive.empty() && !path.empty()) {
      home_env = drive + path;
    }
  }
#else
  if (home_env.empty()) {
    home_env = get_env_var("USERPROFILE");
  }
#endif

  if (!home_env.empty()) {
    std::filesystem::path home(home_env);
    add_dir(home);
    add_dir(home / ".config");
    add_dir(home / ".config" / "autogithubpullmerge");
    add_dir(home / ".agpm");
    add_dir(home / ".autogithubpullmerge");
    add_dir(home / "Documents");
    add_dir(home / "documents");
    add_dir(home / "Desktop");
    add_dir(home / "Downloads");
  }

  auto xdg_config = get_env_var("XDG_CONFIG_HOME");
  if (!xdg_config.empty()) {
    std::filesystem::path xdg(xdg_config);
    add_dir(xdg);
    add_dir(xdg / "autogithubpullmerge");
  }

  auto appdata = get_env_var("APPDATA");
  if (!appdata.empty()) {
    std::filesystem::path roaming(appdata);
    add_dir(roaming);
    add_dir(roaming / "autogithubpullmerge");
  }

  auto local_appdata = get_env_var("LOCALAPPDATA");
  if (!local_appdata.empty()) {
    std::filesystem::path local(local_appdata);
    add_dir(local);
    add_dir(local / "autogithubpullmerge");
  }

  std::vector<std::filesystem::path> discovered;
  std::unordered_set<std::string> seen_files(existing_files.begin(),
                                             existing_files.end());

  for (const auto &dir : search_dirs) {
    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec) {
      continue;
    }
    for (; it != std::filesystem::directory_iterator(); it.increment(ec)) {
      if (ec) {
        ec.clear();
        break;
      }
      const auto &entry = *it;
      if (!entry.is_regular_file(ec)) {
        if (ec) {
          ec.clear();
        }
        continue;
      }
      auto ext = entry.path().extension().string();
      std::string ext_lower;
      ext_lower.reserve(ext.size());
      std::transform(
          ext.begin(), ext.end(), std::back_inserter(ext_lower),
          [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (ext_lower != ".json" && ext_lower != ".yaml" && ext_lower != ".yml" &&
          ext_lower != ".toml") {
        continue;
      }
      auto filename = entry.path().filename().string();
      std::string filename_lower;
      filename_lower.reserve(filename.size());
      std::transform(
          filename.begin(), filename.end(), std::back_inserter(filename_lower),
          [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (filename_lower.find("token") == std::string::npos) {
        continue;
      }
      auto canonical_file = canonicalize_path(entry.path());
      auto key = canonical_file.generic_string();
      if (key.empty() || !seen_files.insert(key).second) {
        continue;
      }
      discovered.push_back(canonical_file);
    }
  }

  std::sort(discovered.begin(), discovered.end(),
            [](const std::filesystem::path &a, const std::filesystem::path &b) {
              return a.generic_string() < b.generic_string();
            });
  return discovered;
}

/**
 * Parse command line arguments into the internal option structure.
 *
 * In addition to CLI11 parsing, this function preprocesses arguments to handle
 * toggle-style flags and fetches tokens from files, URLs, stdin, or
 * environment variables as requested.
 *
 * @param argc Argument count provided to @c main().
 * @param argv Argument vector provided to @c main().
 * @return Fully populated CLI options.
 */
CliOptions parse_cli(int argc, char **argv) {
  CLI::App app{"autogithubpullmerge command line"};
  app.footer(log_category_help_text());
  CLI::Option *request_caddy_flag = nullptr;
  CLI::Option *request_caddy_disable_flag = nullptr;
  CliOptions options;
  std::vector<std::string> raw_args;
  raw_args.reserve(static_cast<std::size_t>(argc));
  for (int i = 0; i < argc; ++i) {
    raw_args.emplace_back(argv[i] != nullptr ? argv[i] : "");
  }
  std::vector<std::string> filtered_args;
  filtered_args.reserve(raw_args.size());
  if (!raw_args.empty()) {
    filtered_args.push_back(raw_args[0]);
  }
  for (std::size_t idx = 1; idx < raw_args.size(); ++idx) {
    const std::string &arg = raw_args[idx];
    if (arg == "--log-compress") {
      options.log_compress = true;
      options.log_compress_explicit = true;
      continue;
    }
    if (arg == "--log-sidecar") {
      options.log_sidecar = true;
      options.log_sidecar_explicit = true;
      continue;
    }
    if (arg == "--no-log-sidecar") {
      options.log_sidecar = false;
      options.log_sidecar_explicit = true;
      continue;
    }
    if (arg == "--request-caddy-window") {
      options.request_caddy_window = true;
      options.request_caddy_explicit = true;
      continue;
    }
    if (arg == "--no-request-caddy-window") {
      options.request_caddy_window = false;
      options.request_caddy_explicit = true;
      continue;
    }
    if (arg == "--open-pat-page") {
      options.open_pat_window = true;
      continue;
    }
    if (arg == "--demo-tui") {
      options.demo_tui = true;
      continue;
    }
    if (arg == "--mcp-server") {
      options.mcp_server_enabled = true;
      options.mcp_server_explicit = true;
      continue;
    }
    if (arg == "--enable-hotkeys" || arg == "-E") {
      options.hotkeys_enabled = true;
      options.hotkeys_explicit = true;
      continue;
    }
    filtered_args.push_back(arg);
  }
  std::string pr_since_str{"0"};
  app.add_flag("-v,--verbose", options.verbose, "Enable verbose output")
      ->group("General");
  app.add_option("-C,--config", options.config_file,
                 "Path to configuration file")
      ->type_name("FILE")
      ->group("General");
  app.add_flag_function(
         "--version",
         [](std::size_t) {
           std::cout << "autogithubpullmerge " << kVersionString << std::endl;
           throw CliParseExit(0);
         },
         "Show version information and exit")
      ->group("General");
  app.add_option(
         "-G,--log-level", options.log_level,
         "Set logging level (trace, debug, info, warn, error, critical, off)")
      ->type_name("LEVEL")
      ->default_val("info")
      ->group("Logging");
  app.add_option("-F,--log-file", options.log_file, "Path to rotating log file")
      ->type_name("FILE")
      ->group("Logging");
  app.add_option("-L,--log-limit", options.log_limit,
                 "Maximum number of log messages to retain")
      ->type_name("N")
      ->default_val("200")
      ->group("Logging");
  app.add_flag("--log-sidecar", "Display logs in a dedicated sidecar window")
      ->group("Logging");
  app.add_flag("--no-log-sidecar",
               "Disable the logger sidecar and use the default layout")
      ->group("Logging");
  request_caddy_flag = app.add_flag(
      "--request-caddy-window", options.request_caddy_window,
      "Show a sidecar window with request queue status and statistics")
                            ->group("UI");
  request_caddy_disable_flag = app.add_flag(
      "--no-request-caddy-window",
      "Disable the request queue sidecar window")
                                   ->group("UI");
  app.add_option_function<std::string>(
         "--log-category",
         [&options](const std::string &value) {
           auto pos = value.find('=');
           std::string name =
               pos == std::string::npos ? value : value.substr(0, pos);
           std::string level = pos == std::string::npos ? std::string{"debug"}
                                                        : value.substr(pos + 1);
           if (name.empty()) {
             throw CLI::ValidationError("--log-category",
                                        "category name must not be empty");
           }
           if (level.empty()) {
             level = "debug";
           }
           options.log_categories[name] = level;
           options.log_categories_explicit = true;
         },
         "Enable a logging category (NAME or NAME=LEVEL). See help footer for "
         "available categories.")
      ->type_name("NAME[=LEVEL]")
      ->group("Logging");
  app.add_option_function<int>(
         "--log-rotate",
         [&options](int value) {
           if (value < 0) {
             throw CLI::ValidationError("--log-rotate",
                                        "rotation count must be non-negative");
           }
           options.log_rotate = value;
           options.log_rotate_explicit = true;
         },
         "Number of rotated log files to retain (0 disables rotation)")
      ->type_name("N")
      ->group("Logging");
  app.add_flag("-y,--yes", options.assume_yes,
               "Assume yes to confirmation prompts")
      ->group("General");
  app.add_flag("-D,--dry-run", options.dry_run,
               "Perform a trial run with no changes")
      ->group("General");
  app.add_flag("--demo-tui", options.demo_tui,
               "Launch interactive demo TUI with mock data")
      ->group("General");
  app.add_flag_function(
         "--enable-hooks",
         [&options](std::size_t) {
           options.hooks_enabled = true;
           options.hooks_explicit = true;
         },
         "Enable hook dispatcher")
      ->group("Hooks");
  app.add_flag_function(
         "--disable-hooks",
         [&options](std::size_t) {
           options.hooks_enabled = false;
           options.hooks_explicit = true;
         },
         "Disable hook dispatcher")
      ->group("Hooks");
  app.add_option_function<std::string>(
         "--hook-command",
         [&options](const std::string &value) {
           options.hook_command = value;
           options.hook_command_explicit = true;
         },
         "Execute COMMAND when hook events fire")
      ->type_name("COMMAND")
      ->group("Hooks");
  app.add_option_function<std::string>(
         "--hook-endpoint",
         [&options](const std::string &value) {
           options.hook_endpoint = value;
           options.hook_endpoint_explicit = true;
         },
         "Send hook events to ENDPOINT")
      ->type_name("URL")
      ->group("Hooks");
  app.add_option_function<std::string>(
         "--hook-method",
         [&options](const std::string &value) {
           options.hook_method = value;
           options.hook_method_explicit = true;
         },
         "HTTP METHOD used for hook endpoint requests")
      ->type_name("METHOD")
      ->group("Hooks");
  app.add_option_function<std::string>(
         "--hook-header",
         [&options](const std::string &value) {
           auto pos = value.find('=');
           if (pos == std::string::npos || pos == 0 ||
               pos + 1 >= value.size()) {
             throw CLI::ValidationError("--hook-header",
                                        "expected NAME=VALUE format");
           }
           std::string name = value.substr(0, pos);
           std::string header_value = value.substr(pos + 1);
           options.hook_headers[name] = header_value;
           options.hook_headers_explicit = true;
         },
         "Add HTTP header NAME=VALUE to hook requests")
      ->type_name("NAME=VALUE")
      ->group("Hooks");
  app.add_option_function<int>(
         "--hook-pull-threshold",
         [&options](int value) {
           if (value < 0) {
             throw CLI::ValidationError("--hook-pull-threshold",
                                        "threshold must be non-negative");
           }
           options.hook_pull_threshold = value;
           options.hook_pull_threshold_explicit = true;
         },
         "Trigger hooks when total pull requests exceed N")
      ->type_name("N")
      ->group("Hooks");
  app.add_option_function<int>(
         "--hook-branch-threshold",
         [&options](int value) {
           if (value < 0) {
             throw CLI::ValidationError("--hook-branch-threshold",
                                        "threshold must be non-negative");
           }
           options.hook_branch_threshold = value;
           options.hook_branch_threshold_explicit = true;
         },
         "Trigger hooks when total branches exceed N")
      ->type_name("N")
      ->group("Hooks");
  app.add_option_function<std::string>(
         "--hotkeys",
         [&options](const std::string &value) {
           std::string lower;
           lower.reserve(value.size());
           std::transform(value.begin(), value.end(), std::back_inserter(lower),
                          [](unsigned char c) {
                            return static_cast<char>(std::tolower(c));
                          });
           if (lower == "on" || lower == "enable" || lower == "enabled" ||
               lower == "true") {
             options.hotkeys_enabled = true;
             options.hotkeys_explicit = true;
           } else if (lower == "off" || lower == "disable" ||
                      lower == "disabled" || lower == "false") {
             options.hotkeys_enabled = false;
             options.hotkeys_explicit = true;
           } else {
             throw CLI::ValidationError(
                 std::string("--hotkeys: expected 'on' or 'off', got '") +
                 value + "'");
           }
         },
         "Explicitly enable or disable interactive hotkeys (on/off)")
      ->type_name("on|off")
      ->group("General");
  app.add_option("-I,--include", options.include_repos,
                 "Repository to include; repeatable")
      ->type_name("REPO")
      ->expected(-1)
      ->group("Repositories");
  app.add_option_function<std::string>(
         "--repo-discovery",
         [&options](const std::string &value) {
           try {
             options.repo_discovery_mode =
                 repo_discovery_mode_from_string(value);
             options.repo_discovery_explicit = true;
           } catch (const std::invalid_argument &e) {
             throw CLI::ValidationError(std::string("--repo-discovery: ") +
                                        e.what());
           }
         },
         "Control repository discovery (default: all; choose from all/filesystem/both/disabled)")
      ->type_name("all|filesystem|both|disabled")
      ->group("Repositories");
  app.add_option("--repo-discovery-root", options.repo_discovery_roots,
                 "Directory to scan for git repositories; repeatable")
      ->type_name("DIR")
      ->expected(-1)
      ->group("Repositories");
  app.add_option("-X,--exclude", options.exclude_repos,
                 "Repository to exclude; repeatable")
      ->type_name("REPO")
      ->expected(-1)
      ->group("Repositories");
  app.add_option("-B,--protect-branch,--protected-branch",
                 options.protected_branches,
                 "Branch pattern to protect from deletion; repeatable")
      ->type_name("PATTERN")
      ->expected(-1)
      ->group("Branch Management");
  app.add_option("-b,--protect-branch-exclude",
                 options.protected_branch_excludes,
                 "Branch pattern to remove protection; repeatable")
      ->type_name("PATTERN")
      ->expected(-1)
      ->group("Branch Management");
  app.add_flag("-m,--include-merged", options.include_merged,
               "Include merged pull requests")
      ->group("Pull Request Management");
  app.add_option("-k,--api-key", options.api_keys,
                 "Personal access token (repeatable, not recommended)")
      ->type_name("TOKEN")
      ->expected(-1)
      ->group("Authentication");
  app.add_flag("-K,--api-key-from-stream", options.api_key_from_stream,
               "Read API key(s) from stdin")
      ->group("Authentication");
  app.add_option("-u,--api-key-url", options.api_key_url,
                 "URL to fetch API key(s)")
      ->type_name("URL")
      ->group("Authentication");
  app.add_option("-U,--api-key-url-user", options.api_key_url_user,
                 "Basic auth username")
      ->type_name("USER")
      ->group("Authentication");
  app.add_option("-P,--api-key-url-password", options.api_key_url_password,
                 "Basic auth password")
      ->type_name("PASS")
      ->group("Authentication");
  app.add_option("-f,--api-key-file", options.api_key_files,
                 "Path to JSON/YAML/TOML file with API key(s); repeatable")
      ->type_name("FILE")
      ->expected(-1)
      ->group("Authentication");
  app.add_flag("--auto-detect-token-files", options.auto_detect_token_files,
               "Search common directories for token files automatically")
      ->group("Authentication");
  app.add_flag("--open-pat-page",
               "Open the GitHub PAT creation page in a browser and exit")
      ->group("Authentication");
  app.add_option("--save-pat", options.pat_save_path,
                 "Write a personal access token to the given file and exit")
      ->type_name("FILE")
      ->group("Authentication");
  app.add_option("--pat-value", options.pat_value,
                 "Personal access token value used with --save-pat")
      ->type_name("TOKEN")
      ->group("Authentication");
  CLI::Option *mcp_bind_option = nullptr;
  CLI::Option *mcp_port_option = nullptr;
  CLI::Option *mcp_backlog_option = nullptr;
  CLI::Option *mcp_max_clients_option = nullptr;
  CLI::Option *mcp_caddy_flag = nullptr;
  app.add_flag("--mcp-server",
               "Enable the Model Context Protocol (MCP) server for "
               "automation integrations")
      ->group("Integrations");
  mcp_bind_option =
      app.add_option("--mcp-server-bind", options.mcp_server_bind_address,
                     "Bind address for the MCP server listener")
          ->type_name("ADDR")
          ->group("Integrations");
  mcp_port_option =
      app.add_option("--mcp-server-port", options.mcp_server_port,
                     "TCP port used by the MCP server")
          ->type_name("PORT")
          ->check(CLI::Range(1, std::numeric_limits<int>::max()))
          ->group("Integrations");
  mcp_backlog_option =
      app.add_option("--mcp-server-backlog", options.mcp_server_backlog,
                     "Pending connection backlog for the MCP listener")
          ->type_name("N")
          ->check(CLI::Range(1, std::numeric_limits<int>::max()))
          ->group("Integrations");
  mcp_max_clients_option = app.add_option(
      "--mcp-server-max-clients", options.mcp_server_max_clients,
      "Maximum clients served per activation (0 = unlimited)")
                                ->type_name("N")
                                ->check(CLI::Range(0, std::numeric_limits<int>::max()))
                                ->group("Integrations");
  mcp_caddy_flag = app.add_flag(
      "--mcp-caddy-window", options.mcp_caddy_window,
      "Show a dedicated window with MCP server activity and statistics")
                       ->group("Integrations");
  app.add_option("-A,--api-base", options.api_base,
                 "Base URL for GitHub API (default: https://api.github.com)")
      ->type_name("URL")
      ->group("Networking");
  app.add_option("-H,--history-db", options.history_db,
                 "Path to SQLite history database")
      ->type_name("FILE")
      ->default_val("history.db")
      ->group("General");
  app.add_option("-c,--export-csv", options.export_csv,
                 "Export pull request history to CSV file after each poll")
      ->type_name("FILE")
      ->group("General");
  app.add_option("-j,--export-json", options.export_json,
                 "Export pull request history to JSON file after each poll")
      ->type_name("FILE")
      ->group("General");
  app.add_option("-p,--poll-interval", options.poll_interval,
                 "Polling interval in seconds")
      ->type_name("SECONDS")
      ->default_val("0")
      ->group("Polling");
  app.add_option("-r,--max-request-rate", options.max_request_rate,
                 "Maximum requests per minute")
      ->type_name("RATE")
      ->default_val("60")
      ->group("Polling");
  app.add_option_function<int>(
         "--max-hourly-requests",
         [&options](int value) {
           if (value < 0) {
             throw CLI::ValidationError(
                 "--max-hourly-requests",
                 "hourly request limit must be non-negative");
           }
           options.max_hourly_requests = value;
           options.max_hourly_requests_explicit = true;
         },
         "Maximum requests per hour (0 uses auto-detected or fallback limit)")
      ->type_name("RATE")
      ->default_str("auto")
      ->group("Polling");
  app.add_option_function<double>(
         "--rate-limit-margin",
         [&options](double value) {
           if (value < 0.0 || value >= 1.0) {
             throw CLI::ValidationError(
                 "--rate-limit-margin",
                 "margin must be between 0 (inclusive) and 1 (exclusive)");
           }
           options.rate_limit_margin = value;
           options.rate_limit_margin_explicit = true;
         },
         "Fraction of the hourly GitHub rate limit to reserve (default 0.7)")
      ->type_name("FRACTION")
      ->group("Polling");
  app.add_option_function<int>(
         "--rate-limit-refresh-interval",
         [&options](int value) {
           if (value <= 0) {
             throw CLI::ValidationError(
                 "--rate-limit-refresh-interval",
                 "refresh interval must be positive seconds");
           }
           options.rate_limit_refresh_interval = value;
           options.rate_limit_refresh_interval_explicit = true;
         },
         "Seconds between GitHub rate limit endpoint checks (default 60)")
      ->type_name("SECONDS")
      ->group("Polling");
  CLI::Option *retry_rate_limit_flag =
      app.add_flag("--retry-rate-limit-endpoint",
                   options.retry_rate_limit_endpoint,
                   "Continue querying the rate limit endpoint after failures")
          ->group("Polling");
  app.add_option_function<int>(
         "--rate-limit-retry-limit",
         [&options](int value) {
           if (value <= 0) {
             throw CLI::ValidationError("--rate-limit-retry-limit",
                                        "retry limit must be positive");
           }
           options.rate_limit_retry_limit = value;
           options.rate_limit_retry_limit_explicit = true;
         },
         "Maximum scheduled retries of the rate limit endpoint when enabled")
      ->type_name("N")
      ->group("Polling");
  app.add_option("-W,--workers", options.workers, "Number of worker threads")
      ->type_name("N")
      ->check(CLI::NonNegativeNumber)
      ->group("Polling");
  app.add_option("-t,--http-timeout", options.http_timeout,
                 "HTTP request timeout in seconds")
      ->type_name("SECONDS")
      ->default_val("30")
      ->group("Networking");
  app.add_option("-R,--http-retries", options.http_retries,
                 "Number of HTTP retry attempts")
      ->type_name("N")
      ->default_val("3")
      ->group("Networking");
  app.add_option("-n,--download-limit", options.download_limit,
                 "Maximum download rate in bytes per second")
      ->type_name("BPS")
      ->group("Networking");
  app.add_option("-o,--upload-limit", options.upload_limit,
                 "Maximum upload rate in bytes per second")
      ->type_name("BPS")
      ->group("Networking");
  app.add_option("-d,--max-download", options.max_download,
                 "Maximum total download in bytes")
      ->type_name("BYTES")
      ->group("Networking");
  app.add_option("-V,--max-upload", options.max_upload,
                 "Maximum total upload in bytes")
      ->type_name("BYTES")
      ->group("Networking");
  app.add_option("-x,--http-proxy", options.http_proxy,
                 "Proxy URL for HTTP requests")
      ->type_name("URL")
      ->group("Networking");
  app.add_option("-z,--https-proxy", options.https_proxy,
                 "Proxy URL for HTTPS requests")
      ->type_name("URL")
      ->group("Networking");
  app.add_flag("-g,--use-graphql", options.use_graphql,
               "Use GraphQL API for pull requests")
      ->group("Networking");
  app.add_option("-Q,--pr-limit", options.pr_limit,
                 "Number of pull requests to fetch")
      ->type_name("N")
      ->default_val("50")
      ->group("Pull Request Management");
  app.add_option("-S,--pr-since", pr_since_str,
                 "Only list pull requests newer than given duration")
      ->type_name("DURATION")
      ->default_val("0")
      ->group("Pull Request Management");
  app.add_option(
         "-O,--single-open-prs", options.single_open_prs_repo,
         "Fetch open PRs for a single repo via one HTTP request and exit")
      ->type_name("OWNER/REPO")
      ->group("Testing");
  app.add_option(
         "-N,--single-branches", options.single_branches_repo,
         "Fetch branches for a single repo via one HTTP request and exit")
      ->type_name("OWNER/REPO")
      ->group("Testing");
  app.add_option(
         "-s,--sort", options.sort,
         "Sort pull requests: alpha, reverse, alphanum, reverse-alphanum")
      ->type_name("MODE")
      ->check(
          CLI::IsMember({"alpha", "reverse", "alphanum", "reverse-alphanum"}))
      ->group("Pull Request Management");
  app.add_flag("-1,--only-poll-prs", options.only_poll_prs,
               "Only poll pull requests")
      ->group("Pull Request Management");
  app.add_flag("-2,--only-poll-stray", options.only_poll_stray,
               "Only poll stray branches")
      ->group("Branch Management");
  app.add_option_function<std::string>(
         "--stray-detection-engine",
         [&options](const std::string &value) {
           auto mode = stray_detection_mode_from_string(value);
           if (!mode) {
             throw CLI::ValidationError(
                 "--stray-detection-engine",
                 "must be one of: rule, heuristic, both");
           }
           options.stray_detection_mode = *mode;
           options.stray_detection_mode_explicit = true;
         },
         "Select stray branch detection engine (rule, heuristic, both)")
      ->type_name("MODE")
      ->group("Branch Management");
  app.add_flag_function(
         "-J,--heuristic-stray-detection",
         [&options](std::int64_t) {
           options.stray_detection_mode = StrayDetectionMode::Combined;
           options.stray_detection_mode_explicit = true;
         },
         "Enable heuristics-based stray branch detection")
      ->group("Branch Management");
  app.add_flag("-3,--reject-dirty", options.reject_dirty,
               "Close dirty stray branches automatically")
      ->group("Branch Management");
  app.add_flag("-4,--delete-stray", options.delete_stray,
               "Delete stray branches without requiring a prefix")
      ->group("Branch Management");
  app.add_flag(
         "-5,--allow-delete-base-branch", options.allow_delete_base_branch,
         "Allow deletion of base branches such as main/master (dangerous)")
      ->group("Branch Management");
  app.add_flag("-6,--auto-merge", options.auto_merge,
               "Automatically merge pull requests")
      ->group("Pull Request Management");
  app.add_option("-7,--require-approval", options.required_approvals,
                 "Minimum number of approvals required before merging")
      ->type_name("N")
      ->default_val("0")
      ->group("Pull Request Management");
  app.add_flag("-8,--require-status-success", options.require_status_success,
               "Require all status checks to succeed before merging")
      ->group("Pull Request Management");
  app.add_flag("-9,--require-mergeable", options.require_mergeable_state,
               "Require pull request to be mergeable")
      ->group("Pull Request Management");
  app.add_option("-0,--purge-prefix", options.purge_prefix,
                 "Delete branches with this prefix after PR close")
      ->type_name("PREFIX")
      ->group("Branch Management");
  app.add_flag("-Y,--purge-only", options.purge_only,
               "Only purge branches and skip PR polling")
      ->group("Branch Management");
  try {
    std::vector<char *> args;
    args.reserve(filtered_args.size() + 1);
    for (auto &s : filtered_args) {
      args.push_back(const_cast<char *>(s.c_str()));
    }
    args.push_back(nullptr);
    int parse_argc = static_cast<int>(filtered_args.size());
    app.parse(parse_argc, args.data());
  } catch (const CLI::ParseError &e) {
    int exit_code = app.exit(e);
    throw CliParseExit(exit_code);
  }
  if (mcp_bind_option != nullptr && mcp_bind_option->count() > 0U) {
    options.mcp_server_bind_explicit = true;
  }
  if (mcp_port_option != nullptr && mcp_port_option->count() > 0U) {
    options.mcp_server_port_explicit = true;
  }
  if (mcp_backlog_option != nullptr && mcp_backlog_option->count() > 0U) {
    options.mcp_server_backlog_explicit = true;
  }
  if (mcp_max_clients_option != nullptr &&
      mcp_max_clients_option->count() > 0U) {
    options.mcp_server_max_clients_explicit = true;
  }
  if (mcp_caddy_flag != nullptr && mcp_caddy_flag->count() > 0U) {
    options.mcp_caddy_explicit = true;
  }
  if (request_caddy_flag != nullptr && request_caddy_flag->count() > 0U) {
    options.request_caddy_window = true;
    options.request_caddy_explicit = true;
  }
  if (request_caddy_disable_flag != nullptr &&
      request_caddy_disable_flag->count() > 0U) {
    options.request_caddy_window = false;
    options.request_caddy_explicit = true;
  }
  if (retry_rate_limit_flag != nullptr && retry_rate_limit_flag->count() > 0U) {
    options.retry_rate_limit_endpoint_explicit = true;
  }
  std::unordered_set<std::string> canonical_token_files;
  canonical_token_files.reserve(options.api_key_files.size());
  for (const auto &token_file : options.api_key_files) {
    auto canonical = canonicalize_path(token_file);
    auto canonical_str = canonical.generic_string();
    if (!canonical_str.empty()) {
      canonical_token_files.insert(canonical_str);
    }
    auto tokens = load_tokens_from_file(token_file);
    options.api_keys.insert(options.api_keys.end(), tokens.begin(),
                            tokens.end());
  }
  if (options.auto_detect_token_files) {
    std::filesystem::path exe_arg;
    if (!filtered_args.empty()) {
      exe_arg = filtered_args.front();
    }
    auto discovered = discover_token_files(
        exe_arg, options.repo_discovery_roots, canonical_token_files);
    for (const auto &path : discovered) {
      auto canonical = canonicalize_path(path);
      auto canonical_str = canonical.generic_string();
      if (!canonical_str.empty()) {
        canonical_token_files.insert(canonical_str);
      }
      auto path_string = canonical.string();
      if (path_string.empty()) {
        path_string = path.string();
      }
      options.auto_detected_api_key_files.push_back(path_string);
      try {
        auto tokens = load_tokens_from_file(path_string);
        options.api_keys.insert(options.api_keys.end(), tokens.begin(),
                                tokens.end());
      } catch (const std::exception &e) {
        cli_log()->warn("Failed to load auto-detected token file {}: {}",
                        path_string, e.what());
      }
    }
  }
  if (!options.api_key_url.empty()) {
    auto tokens =
        load_tokens_from_url(options.api_key_url, options.api_key_url_user,
                             options.api_key_url_password);
    options.api_keys.insert(options.api_keys.end(), tokens.begin(),
                            tokens.end());
  }
  if (!options.pat_value.empty() && options.pat_save_path.empty()) {
    throw CLI::ValidationError(
        "--pat-value requires --save-pat to specify an output file");
  }
  if (options.open_pat_window && !options.pat_save_path.empty()) {
    throw CLI::ValidationError(
        "--open-pat-page cannot be combined with --save-pat");
  }
  if (options.api_key_from_stream) {
    std::string line;
    while (std::getline(std::cin, line)) {
      if (!line.empty()) {
        options.api_keys.push_back(line);
      }
    }
  }
  if (options.api_keys.empty()) {
    auto env = get_env_var("GITHUB_TOKEN");
    if (env.empty()) {
      env = get_env_var("AGPM_API_KEY");
    }
    if (!env.empty()) {
      options.api_keys.emplace_back(env);
    }
  }
  options.pr_since = parse_duration(pr_since_str);
  return options;
}

} // namespace agpm
