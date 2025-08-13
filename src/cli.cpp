#include "cli.hpp"
#include "curl/curl.h"
#include <CLI/CLI.hpp>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <yaml-cpp/yaml.h>

namespace agpm {

static size_t write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t total = size * nmemb;
  std::string *s = static_cast<std::string *>(userp);
  s->append(static_cast<char *>(contents), total);
  return total;
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
      for (const auto &n : node) {
        tokens.push_back(n.as<std::string>());
      }
    }
    if (node["token"]) {
      tokens.push_back(node["token"].as<std::string>());
    }
    if (node["tokens"]) {
      for (const auto &n : node["tokens"]) {
        tokens.push_back(n.as<std::string>());
      }
    }
  } else if (ext == "json") {
    std::ifstream f(path);
    if (!f) {
      throw std::runtime_error("Failed to open token file");
    }
    nlohmann::json j;
    f >> j;
    if (j.is_array()) {
      for (const auto &item : j) {
        tokens.push_back(item.get<std::string>());
      }
    } else {
      if (j.contains("token")) {
        tokens.push_back(j["token"].get<std::string>());
      }
      if (j.contains("tokens")) {
        for (const auto &item : j["tokens"]) {
          tokens.push_back(item.get<std::string>());
        }
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
  CURLcode res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  if (res != CURLE_OK) {
    throw std::runtime_error("curl GET failed");
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

static std::chrono::seconds parse_duration(const std::string &str) {
  if (str.empty())
    return std::chrono::seconds{0};
  char unit = str.back();
  std::string num = str;
  if (!std::isdigit(static_cast<unsigned char>(unit))) {
    num.pop_back();
  } else {
    unit = 's';
  }
  long value = std::stol(num);
  switch (std::tolower(static_cast<unsigned char>(unit))) {
  case 's':
    return std::chrono::seconds{value};
  case 'm':
    return std::chrono::seconds{value * 60};
  case 'h':
    return std::chrono::seconds{value * 3600};
  case 'd':
    return std::chrono::seconds{value * 86400};
  case 'w':
    return std::chrono::seconds{value * 604800};
  default:
    throw std::runtime_error("Invalid duration suffix");
  }
}

CliOptions parse_cli(int argc, char **argv) {
  CLI::App app{"autogithubpullmerge command line"};
  CliOptions options;
  std::string pr_since_str{"0"};
  app.add_flag("-v,--verbose", options.verbose, "Enable verbose output");
  app.add_option("--config", options.config_file, "Path to configuration file")
      ->type_name("FILE");
  app.add_option(
         "--log-level", options.log_level,
         "Set logging level (trace, debug, info, warn, error, critical, off)")
      ->type_name("LEVEL")
      ->default_val("info");
  app.add_option("--include", options.include_repos,
                 "Repository to include; repeatable")
      ->type_name("REPO")
      ->expected(-1);
  app.add_option("--exclude", options.exclude_repos,
                 "Repository to exclude; repeatable")
      ->type_name("REPO")
      ->expected(-1);
  app.add_flag("--include-merged", options.include_merged,
               "Include merged pull requests");
  app.add_option("--api-key", options.api_keys,
                 "Personal access token (repeatable, not recommended)")
      ->type_name("TOKEN")
      ->expected(-1);
  app.add_flag("--api-key-from-stream", options.api_key_from_stream,
               "Read API key(s) from stdin");
  app.add_option("--api-key-url", options.api_key_url,
                 "URL to fetch API key(s)")
      ->type_name("URL");
  app.add_option("--api-key-url-user", options.api_key_url_user,
                 "Basic auth username")
      ->type_name("USER");
  app.add_option("--api-key-url-password", options.api_key_url_password,
                 "Basic auth password")
      ->type_name("PASS");
  app.add_option("--api-key-file", options.api_key_file,
                 "Path to JSON/YAML file with API key(s)")
      ->type_name("FILE");
  app.add_option("--history-db", options.history_db,
                 "Path to SQLite history database")
      ->type_name("FILE")
      ->default_val("history.db");
  app.add_option("--poll-interval", options.poll_interval,
                 "Polling interval in seconds")
      ->type_name("SECONDS")
      ->default_val("0");
  app.add_option("--max-request-rate", options.max_request_rate,
                 "Maximum requests per minute")
      ->type_name("RATE")
      ->default_val("60");
  app.add_option("--pr-limit", options.pr_limit,
                 "Number of pull requests to fetch")
      ->type_name("N")
      ->default_val("50");
  app.add_option("--pr-since", pr_since_str,
                 "Only list pull requests newer than given duration")
      ->type_name("DURATION")
      ->default_val("0");
  app.add_flag("--only-poll-prs", options.only_poll_prs,
               "Only poll pull requests");
  app.add_flag("--only-poll-stray", options.only_poll_stray,
               "Only poll stray branches");
  app.add_flag("--reject-dirty", options.reject_dirty,
               "Close dirty stray branches automatically");
  app.add_flag("--auto-merge", options.auto_merge,
               "Automatically merge pull requests");
  app.add_option("--purge-prefix", options.purge_prefix,
                 "Delete branches with this prefix after PR close")
      ->type_name("PREFIX");
  app.add_flag("--purge-only", options.purge_only,
               "Only purge branches and skip PR polling");
  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    exit(app.exit(e));
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
  options.pr_since = parse_duration(pr_since_str);
  return options;
}

} // namespace agpm
