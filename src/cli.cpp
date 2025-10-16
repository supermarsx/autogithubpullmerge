#include "cli.hpp"
#include "curl/curl.h"
#include "util/duration.hpp"
#include <CLI/CLI.hpp>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <iterator>
#include <yaml-cpp/yaml.h>

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

static std::vector<std::string> load_tokens_from_file(const std::string &path) {
  auto pos = path.find_last_of('.');
  if (pos == std::string::npos) {
    throw std::runtime_error("Unknown token file extension");
  }
  std::string ext = path.substr(pos + 1);
  std::vector<std::string> tokens;
  if (ext == "yaml" || ext == "yml") {
    YAML::Node node = YAML::LoadFile(path);
    if (node.IsSequence()) {
      tokens.reserve(tokens.size() + node.size());
      std::transform(node.begin(), node.end(), std::back_inserter(tokens),
                     [](const YAML::Node &n) { return n.as<std::string>(); });
    }
    if (node["token"]) {
      tokens.push_back(node["token"].as<std::string>());
    }
    if (node["tokens"]) {
      const YAML::Node tokens_node = node["tokens"];
      tokens.reserve(tokens.size() + tokens_node.size());
      std::transform(tokens_node.begin(), tokens_node.end(),
                     std::back_inserter(tokens),
                     [](const YAML::Node &n) { return n.as<std::string>(); });
    }
  } else if (ext == "json") {
    std::ifstream f(path);
    if (!f) {
      throw std::runtime_error("Failed to open token file");
    }
    nlohmann::json j;
    f >> j;
    if (j.is_array()) {
      tokens.reserve(tokens.size() + j.size());
      std::transform(j.begin(), j.end(), std::back_inserter(tokens),
                     [](const nlohmann::json &item) {
                       return item.get<std::string>();
                     });
    } else {
      if (j.contains("token")) {
        tokens.push_back(j["token"].get<std::string>());
      }
      if (j.contains("tokens")) {
        const auto &array = j["tokens"];
        tokens.reserve(tokens.size() + array.size());
        std::transform(array.begin(), array.end(), std::back_inserter(tokens),
                       [](const nlohmann::json &item) {
                         return item.get<std::string>();
                       });
      }
    }
  } else {
    throw std::runtime_error("Unsupported token file format");
  }
  return tokens;
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
      ->group("General");
  app.add_option("-F,--log-file", options.log_file,
                 "Path to rotating log file")
      ->type_name("FILE")
      ->group("General");
  app.add_option("-L,--log-limit", options.log_limit,
                 "Maximum number of log messages to retain")
      ->type_name("N")
      ->default_val("200")
      ->group("General");
  app.add_flag("-y,--yes", options.assume_yes,
               "Assume yes to confirmation prompts")
      ->group("General");
  app.add_flag("-D,--dry-run", options.dry_run,
               "Perform a trial run with no changes")
      ->group("General");
  app.add_option("-I,--include", options.include_repos,
                 "Repository to include; repeatable")
      ->type_name("REPO")
      ->expected(-1)
      ->group("Repositories");
  app.add_option("-X,--exclude", options.exclude_repos,
                 "Repository to exclude; repeatable")
      ->type_name("REPO")
      ->expected(-1)
      ->group("Repositories");
  auto protect_branch = app.add_option("-B,--protect-branch,--protected-branch",
                 options.protected_branches,
                 "Branch pattern to protect from deletion; repeatable")
      ->type_name("PATTERN")
      ->expected(-1)
      ->group("Branch Management");
  auto protect_branch_exclude =
      app.add_option("--protect-branch-exclude",
                 "Branch pattern to remove protection; repeatable")
      ->type_name("PATTERN")
      ->expected(-1)
      ->group("Branch Management");
  app.add_flag("-m,--include-merged", options.include_merged,
               "Include merged pull requests")
      ->group("Pull Request Management");
  auto api_key_opt =
      app.add_option("-k,--api-key", options.api_keys,
                 "Personal access token (repeatable, not recommended)")
      ->type_name("TOKEN")
      ->expected(-1)
      ->group("Authentication");
  auto api_key_stream =
      app.add_flag("--api-key-from-stream", options.api_key_from_stream,
               "Read API key(s) from stdin")
      ->group("Authentication");
  auto api_key_url =
      app.add_option("--api-key-url", options.api_key_url,
                 "URL to fetch API key(s)")
      ->type_name("URL")
      ->group("Authentication");
  auto api_key_url_user =
      app.add_option("--api-key-url-user", options.api_key_url_user,
                 "Basic auth username")
      ->type_name("USER")
      ->group("Authentication");
  auto api_key_url_password = app.add_option(
      "--api-key-url-password", options.api_key_url_password,
                 "Basic auth password")
      ->type_name("PASS")
      ->group("Authentication");
  auto api_key_file =
      app.add_option("--api-key-file", options.api_key_file,
                 "Path to JSON/YAML file with API key(s)")
      ->type_name("FILE")
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
  auto export_csv =
      app.add_option("--export-csv", options.export_csv,
                 "Export pull request history to CSV file after each poll")
      ->type_name("FILE")
      ->group("General");
  auto export_json =
      app.add_option("--export-json", options.export_json,
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
  auto http_retries =
      app.add_option("--http-retries", options.http_retries,
                 "Number of HTTP retry attempts")
      ->type_name("N")
      ->default_val("3")
      ->group("Networking");
  auto download_limit =
      app.add_option("--download-limit", options.download_limit,
                 "Maximum download rate in bytes per second")
      ->type_name("BPS")
      ->group("Networking");
  auto upload_limit =
      app.add_option("--upload-limit", options.upload_limit,
                 "Maximum upload rate in bytes per second")
      ->type_name("BPS")
      ->group("Networking");
  auto max_download =
      app.add_option("--max-download", options.max_download,
                 "Maximum total download in bytes")
      ->type_name("BYTES")
      ->group("Networking");
  auto max_upload =
      app.add_option("--max-upload", options.max_upload,
                 "Maximum total upload in bytes")
      ->type_name("BYTES")
      ->group("Networking");
  auto http_proxy = app.add_option("--http-proxy", options.http_proxy,
                 "Proxy URL for HTTP requests")
      ->type_name("URL")
      ->group("Networking");
  auto https_proxy =
      app.add_option("--https-proxy", options.https_proxy,
                 "Proxy URL for HTTPS requests")
      ->type_name("URL")
      ->group("Networking");
  app.add_flag("-g,--use-graphql", options.use_graphql,
               "Use GraphQL API for pull requests")
      ->group("Networking");
  auto pr_limit =
      app.add_option("--pr-limit", options.pr_limit,
                 "Number of pull requests to fetch")
      ->type_name("N")
      ->default_val("50")
      ->group("Pull Request Management");
  auto pr_since = app.add_option("--pr-since", pr_since_str,
                 "Only list pull requests newer than given duration")
      ->type_name("DURATION")
      ->default_val("0")
      ->group("Pull Request Management");
  auto single_open_prs = app.add_option(
         "--single-open-prs", options.single_open_prs_repo,
         "Fetch open PRs for a single repo via one HTTP request and exit")
      ->type_name("OWNER/REPO")
      ->group("Testing");
  auto single_branches = app.add_option(
         "--single-branches", options.single_branches_repo,
         "Fetch branches for a single repo via one HTTP request and exit")
      ->type_name("OWNER/REPO")
      ->group("Testing");
  auto sort_opt = app.add_option(
         "-S,--sort", options.sort,
         "Sort pull requests: alpha, reverse, alphanum, reverse-alphanum")
      ->type_name("MODE")
      ->check(
          CLI::IsMember({"alpha", "reverse", "alphanum", "reverse-alphanum"}))
      ->group("Pull Request Management");
  auto only_poll_prs =
      app.add_flag("--only-poll-prs", options.only_poll_prs,
               "Only poll pull requests")
      ->group("Pull Request Management");
  auto only_poll_stray =
      app.add_flag("--only-poll-stray", options.only_poll_stray,
               "Only poll stray branches")
      ->group("Branch Management");
  auto reject_dirty =
      app.add_flag("--reject-dirty", options.reject_dirty,
               "Close dirty stray branches automatically")
      ->group("Branch Management");
  auto delete_stray =
      app.add_flag("--delete-stray", options.delete_stray,
               "Delete stray branches without requiring a prefix")
      ->group("Branch Management");
  app.add_flag("--allow-delete-base-branch", options.allow_delete_base_branch,
               "Allow deletion of base branches such as main/master (dangerous)")
      ->group("Branch Management");
  auto auto_merge =
      app.add_flag("--auto-merge", options.auto_merge,
               "Automatically merge pull requests")
      ->group("Pull Request Management");
  auto require_approval =
      app.add_option("--require-approval", options.required_approvals,
                 "Minimum number of approvals required before merging")
      ->type_name("N")
      ->default_val("0")
      ->group("Pull Request Management");
  auto require_status_success = app.add_flag(
      "--require-status-success", options.require_status_success,
               "Require all status checks to succeed before merging")
      ->group("Pull Request Management");
  auto require_mergeable =
      app.add_flag("--require-mergeable", options.require_mergeable_state,
               "Require pull request to be mergeable")
      ->group("Pull Request Management");
  auto purge_prefix =
      app.add_option("--purge-prefix", options.purge_prefix,
                 "Delete branches with this prefix after PR close")
      ->type_name("PREFIX")
      ->group("Branch Management");
  auto purge_only =
      app.add_flag("--purge-only", options.purge_only,
               "Only purge branches and skip PR polling")
      ->group("Branch Management");
  protect_branch->alias("-pb");
  protect_branch_exclude->alias("-px");
  api_key_opt->alias("-ak");
  api_key_stream->alias("-ks");
  api_key_url->alias("-ku");
  api_key_url_user->alias("-kU");
  api_key_url_password->alias("-kp");
  api_key_file->alias("-kf");
  export_csv->alias("-ec");
  export_json->alias("-ej");
  http_retries->alias("-hr");
  download_limit->alias("-dl");
  upload_limit->alias("-ul");
  max_download->alias("-md");
  max_upload->alias("-mu");
  http_proxy->alias("-hp");
  https_proxy->alias("-hs");
  pr_limit->alias("-pl");
  pr_since->alias("-ps");
  single_open_prs->alias("-so");
  single_branches->alias("-sb");
  only_poll_prs->alias("-op");
  only_poll_stray->alias("-os");
  reject_dirty->alias("-rd");
  delete_stray->alias("-ds");
  auto_merge->alias("-am");
  require_approval->alias("-ra");
  require_status_success->alias("-rs");
  require_mergeable->alias("-rm");
  purge_prefix->alias("-pp");
  purge_only->alias("-po");
  try {
    std::vector<char *> args(argv, argv + argc);
    args.push_back(nullptr);
    app.parse(argc, args.data());
  } catch (const CLI::ParseError &e) {
    int exit_code = app.exit(e);
    throw CliParseExit(exit_code);
  }
  if (!options.api_key_file.empty()) {
    auto tokens = load_tokens_from_file(options.api_key_file);
    options.api_keys.insert(options.api_keys.end(), tokens.begin(),
                            tokens.end());
  }
  if (!options.api_key_url.empty()) {
    auto tokens =
        load_tokens_from_url(options.api_key_url, options.api_key_url_user,
                             options.api_key_url_password);
    options.api_keys.insert(options.api_keys.end(), tokens.begin(),
                            tokens.end());
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
  bool destructive =
      (options.reject_dirty || options.delete_stray ||
       options.allow_delete_base_branch || options.auto_merge ||
       !options.purge_prefix.empty() || options.purge_only) &&
      !options.dry_run;
  if (destructive && !options.assume_yes) {
    std::cout << "Destructive options enabled. Continue? [y/N]: ";
    std::string resp;
    std::getline(std::cin, resp);
    if (!(resp == "y" || resp == "Y" || resp == "yes" || resp == "YES")) {
      throw std::runtime_error("Operation cancelled by user");
    }
  }
  return options;
}

} // namespace agpm
