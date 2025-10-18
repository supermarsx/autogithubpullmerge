#include "cli.hpp"
#include "curl/curl.h"
#include "token_loader.hpp"
#include "util/duration.hpp"
#include <CLI/CLI.hpp>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace agpm {

static size_t write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t total = size * nmemb;
  std::string *s = static_cast<std::string *>(userp);
  s->append(static_cast<char *>(contents), total);
  return total;
}

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

// Fetches an environment variable in a cross-platform, secure manner.
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

CliOptions parse_cli(int argc, char **argv) {
  CLI::App app{"autogithubpullmerge command line"};
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
    if (arg == "--no-log-compress") {
      options.log_compress = false;
      options.log_compress_explicit = true;
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
  app.add_option(
         "-G,--log-level", options.log_level,
         "Set logging level (trace, debug, info, warn, error, critical, off)")
      ->type_name("LEVEL")
      ->default_val("info")
      ->group("Logging");
  app.add_option("-F,--log-file", options.log_file,
                 "Path to rotating log file")
      ->type_name("FILE")
      ->group("Logging");
  app.add_option("-L,--log-limit", options.log_limit,
                 "Maximum number of log messages to retain")
      ->type_name("N")
      ->default_val("200")
      ->group("Logging");
  app
      .add_option_function<int>(
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
  app
      .add_option_function<std::string>(
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
  app
      .add_option_function<std::string>(
          "--repo-discovery",
          [&options](const std::string &value) {
            try {
              options.repo_discovery_mode = repo_discovery_mode_from_string(value);
              options.repo_discovery_explicit = true;
            } catch (const std::invalid_argument &e) {
              throw CLI::ValidationError(
                  std::string("--repo-discovery: ") + e.what());
            }
          },
          "Control repository discovery (disabled/all/filesystem)")
      ->type_name("disabled|all|filesystem")
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
  app.add_option("-W,--workers", options.workers,
                 "Number of worker threads")
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
  app.add_option("-O,--single-open-prs", options.single_open_prs_repo,
                 "Fetch open PRs for a single repo via one HTTP request and exit")
      ->type_name("OWNER/REPO")
      ->group("Testing");
  app.add_option("-N,--single-branches", options.single_branches_repo,
                 "Fetch branches for a single repo via one HTTP request and exit")
      ->type_name("OWNER/REPO")
      ->group("Testing");
  app.add_option("-s,--sort", options.sort,
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
  app.add_flag("-3,--reject-dirty", options.reject_dirty,
               "Close dirty stray branches automatically")
      ->group("Branch Management");
  app.add_flag("-4,--delete-stray", options.delete_stray,
               "Delete stray branches without requiring a prefix")
      ->group("Branch Management");
  app.add_flag("-5,--allow-delete-base-branch", options.allow_delete_base_branch,
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
  for (const auto &token_file : options.api_key_files) {
    auto tokens = load_tokens_from_file(token_file);
    options.api_keys.insert(options.api_keys.end(), tokens.begin(), tokens.end());
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
