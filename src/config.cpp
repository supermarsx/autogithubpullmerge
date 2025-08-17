#include "config.hpp"
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

namespace agpm {

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

Config Config::from_file(const std::string &path) {
  Config cfg;
  auto pos = path.find_last_of('.');
  if (pos == std::string::npos) {
    throw std::runtime_error("Unknown config file extension");
  }
  std::string ext = path.substr(pos + 1);
  if (ext == "yaml" || ext == "yml") {
    YAML::Node node = YAML::LoadFile(path);
    if (node["verbose"]) {
      cfg.set_verbose(node["verbose"].as<bool>());
    }
    if (node["poll_interval"]) {
      cfg.set_poll_interval(node["poll_interval"].as<int>());
    }
    if (node["max_request_rate"]) {
      cfg.set_max_request_rate(node["max_request_rate"].as<int>());
    }
    if (node["http_timeout"]) {
      cfg.set_http_timeout(node["http_timeout"].as<int>());
    }
    if (node["http_retries"]) {
      cfg.set_http_retries(node["http_retries"].as<int>());
    }
    if (node["log_level"]) {
      cfg.set_log_level(node["log_level"].as<std::string>());
    }
    if (node["log_pattern"]) {
      cfg.set_log_pattern(node["log_pattern"].as<std::string>());
    }
    if (node["log_file"]) {
      cfg.set_log_file(node["log_file"].as<std::string>());
    }
    if (node["include_repos"]) {
      cfg.set_include_repos(
          node["include_repos"].as<std::vector<std::string>>());
    }
    if (node["exclude_repos"]) {
      cfg.set_exclude_repos(
          node["exclude_repos"].as<std::vector<std::string>>());
    }
    if (node["include_merged"]) {
      cfg.set_include_merged(node["include_merged"].as<bool>());
    }
    if (node["api_keys"]) {
      cfg.set_api_keys(node["api_keys"].as<std::vector<std::string>>());
    }
    if (node["api_key_from_stream"]) {
      cfg.set_api_key_from_stream(node["api_key_from_stream"].as<bool>());
    }
    if (node["api_key_url"]) {
      cfg.set_api_key_url(node["api_key_url"].as<std::string>());
    }
    if (node["api_key_url_user"]) {
      cfg.set_api_key_url_user(node["api_key_url_user"].as<std::string>());
    }
    if (node["api_key_url_password"]) {
      cfg.set_api_key_url_password(
          node["api_key_url_password"].as<std::string>());
    }
    if (node["api_key_file"]) {
      cfg.set_api_key_file(node["api_key_file"].as<std::string>());
    }
    if (node["history_db"]) {
      cfg.set_history_db(node["history_db"].as<std::string>());
    }
    if (node["only_poll_prs"]) {
      cfg.set_only_poll_prs(node["only_poll_prs"].as<bool>());
    }
    if (node["only_poll_stray"]) {
      cfg.set_only_poll_stray(node["only_poll_stray"].as<bool>());
    }
    if (node["reject_dirty"]) {
      cfg.set_reject_dirty(node["reject_dirty"].as<bool>());
    }
    if (node["auto_merge"]) {
      cfg.set_auto_merge(node["auto_merge"].as<bool>());
    }
    if (node["purge_prefix"]) {
      cfg.set_purge_prefix(node["purge_prefix"].as<std::string>());
    }
    if (node["pr_limit"]) {
      cfg.set_pr_limit(node["pr_limit"].as<int>());
    }
    if (node["pr_since"]) {
      cfg.set_pr_since(parse_duration(node["pr_since"].as<std::string>()));
    }
  } else if (ext == "json") {
    std::ifstream f(path);
    if (!f) {
      throw std::runtime_error("Failed to open config file");
    }
    nlohmann::json j;
    f >> j;
    if (j.contains("verbose")) {
      cfg.set_verbose(j["verbose"].get<bool>());
    }
    if (j.contains("poll_interval")) {
      cfg.set_poll_interval(j["poll_interval"].get<int>());
    }
    if (j.contains("max_request_rate")) {
      cfg.set_max_request_rate(j["max_request_rate"].get<int>());
    }
    if (j.contains("http_timeout")) {
      cfg.set_http_timeout(j["http_timeout"].get<int>());
    }
    if (j.contains("http_retries")) {
      cfg.set_http_retries(j["http_retries"].get<int>());
    }
    if (j.contains("log_level")) {
      cfg.set_log_level(j["log_level"].get<std::string>());
    }
    if (j.contains("log_pattern")) {
      cfg.set_log_pattern(j["log_pattern"].get<std::string>());
    }
    if (j.contains("log_file")) {
      cfg.set_log_file(j["log_file"].get<std::string>());
    }
    if (j.contains("include_repos")) {
      cfg.set_include_repos(j["include_repos"].get<std::vector<std::string>>());
    }
    if (j.contains("exclude_repos")) {
      cfg.set_exclude_repos(j["exclude_repos"].get<std::vector<std::string>>());
    }
    if (j.contains("include_merged")) {
      cfg.set_include_merged(j["include_merged"].get<bool>());
    }
    if (j.contains("api_keys")) {
      cfg.set_api_keys(j["api_keys"].get<std::vector<std::string>>());
    }
    if (j.contains("api_key_from_stream")) {
      cfg.set_api_key_from_stream(j["api_key_from_stream"].get<bool>());
    }
    if (j.contains("api_key_url")) {
      cfg.set_api_key_url(j["api_key_url"].get<std::string>());
    }
    if (j.contains("api_key_url_user")) {
      cfg.set_api_key_url_user(j["api_key_url_user"].get<std::string>());
    }
    if (j.contains("api_key_url_password")) {
      cfg.set_api_key_url_password(
          j["api_key_url_password"].get<std::string>());
    }
    if (j.contains("api_key_file")) {
      cfg.set_api_key_file(j["api_key_file"].get<std::string>());
    }
    if (j.contains("history_db")) {
      cfg.set_history_db(j["history_db"].get<std::string>());
    }
    if (j.contains("only_poll_prs")) {
      cfg.set_only_poll_prs(j["only_poll_prs"].get<bool>());
    }
    if (j.contains("only_poll_stray")) {
      cfg.set_only_poll_stray(j["only_poll_stray"].get<bool>());
    }
    if (j.contains("reject_dirty")) {
      cfg.set_reject_dirty(j["reject_dirty"].get<bool>());
    }
    if (j.contains("auto_merge")) {
      cfg.set_auto_merge(j["auto_merge"].get<bool>());
    }
    if (j.contains("purge_prefix")) {
      cfg.set_purge_prefix(j["purge_prefix"].get<std::string>());
    }
    if (j.contains("pr_limit")) {
      cfg.set_pr_limit(j["pr_limit"].get<int>());
    }
    if (j.contains("pr_since")) {
      cfg.set_pr_since(parse_duration(j["pr_since"].get<std::string>()));
    }
  } else {
    throw std::runtime_error("Unsupported config format");
  }
  return cfg;
}

} // namespace agpm
