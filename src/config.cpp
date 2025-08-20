#include "config.hpp"
#include "util/duration.hpp"
#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

namespace agpm {

namespace {

nlohmann::json yaml_to_json(const YAML::Node &node) {
  using nlohmann::json;
  switch (node.Type()) {
  case YAML::NodeType::Null:
    return nullptr;
  case YAML::NodeType::Scalar: {
    const std::string s = node.Scalar();
    if (s == "true" || s == "True" || s == "TRUE")
      return true;
    if (s == "false" || s == "False" || s == "FALSE")
      return false;
    try {
      size_t idx = 0;
      long long i = std::stoll(s, &idx, 0);
      if (idx == s.size())
        return i;
    } catch (...) {
    }
    try {
      size_t idx = 0;
      double d = std::stod(s, &idx);
      if (idx == s.size())
        return d;
    } catch (...) {
    }
    return s;
  }
  case YAML::NodeType::Sequence: {
    json arr = json::array();
    for (const auto &item : node) {
      arr.push_back(yaml_to_json(item));
    }
    return arr;
  }
  case YAML::NodeType::Map: {
    json obj = json::object();
    for (const auto &kv : node) {
      obj[kv.first.as<std::string>()] = yaml_to_json(kv.second);
    }
    return obj;
  }
  default:
    return nullptr;
  }
}

} // namespace

void Config::load_json(const nlohmann::json &j) {
  if (j.contains("verbose")) {
    set_verbose(j["verbose"].get<bool>());
  }
  if (j.contains("poll_interval")) {
    set_poll_interval(j["poll_interval"].get<int>());
  }
  if (j.contains("max_request_rate")) {
    set_max_request_rate(j["max_request_rate"].get<int>());
  }
  if (j.contains("http_timeout")) {
    set_http_timeout(j["http_timeout"].get<int>());
  }
  if (j.contains("http_retries")) {
    set_http_retries(j["http_retries"].get<int>());
  }
  if (j.contains("download_limit")) {
    set_download_limit(j["download_limit"].get<long long>());
  }
  if (j.contains("upload_limit")) {
    set_upload_limit(j["upload_limit"].get<long long>());
  }
  if (j.contains("max_download")) {
    set_max_download(j["max_download"].get<long long>());
  }
  if (j.contains("max_upload")) {
    set_max_upload(j["max_upload"].get<long long>());
  }
  if (j.contains("log_level")) {
    set_log_level(j["log_level"].get<std::string>());
  }
  if (j.contains("log_pattern")) {
    set_log_pattern(j["log_pattern"].get<std::string>());
  }
  if (j.contains("log_file")) {
    set_log_file(j["log_file"].get<std::string>());
  }
  if (j.contains("include_repos")) {
    set_include_repos(j["include_repos"].get<std::vector<std::string>>());
  }
  if (j.contains("exclude_repos")) {
    set_exclude_repos(j["exclude_repos"].get<std::vector<std::string>>());
  }
  if (j.contains("include_merged")) {
    set_include_merged(j["include_merged"].get<bool>());
  }
  if (j.contains("api_keys")) {
    set_api_keys(j["api_keys"].get<std::vector<std::string>>());
  }
  if (j.contains("api_key_from_stream")) {
    set_api_key_from_stream(j["api_key_from_stream"].get<bool>());
  }
  if (j.contains("api_key_url")) {
    set_api_key_url(j["api_key_url"].get<std::string>());
  }
  if (j.contains("api_key_url_user")) {
    set_api_key_url_user(j["api_key_url_user"].get<std::string>());
  }
  if (j.contains("api_key_url_password")) {
    set_api_key_url_password(j["api_key_url_password"].get<std::string>());
  }
  if (j.contains("api_key_file")) {
    set_api_key_file(j["api_key_file"].get<std::string>());
  }
  if (j.contains("history_db")) {
    set_history_db(j["history_db"].get<std::string>());
  }
  if (j.contains("only_poll_prs")) {
    set_only_poll_prs(j["only_poll_prs"].get<bool>());
  }
  if (j.contains("only_poll_stray")) {
    set_only_poll_stray(j["only_poll_stray"].get<bool>());
  }
  if (j.contains("purge_only")) {
    set_purge_only(j["purge_only"].get<bool>());
  }
  if (j.contains("reject_dirty")) {
    set_reject_dirty(j["reject_dirty"].get<bool>());
  }
  if (j.contains("auto_merge")) {
    set_auto_merge(j["auto_merge"].get<bool>());
  }
  if (j.contains("purge_prefix")) {
    set_purge_prefix(j["purge_prefix"].get<std::string>());
  }
  if (j.contains("pr_limit")) {
    set_pr_limit(j["pr_limit"].get<int>());
  }
  if (j.contains("pr_since")) {
    set_pr_since(parse_duration(j["pr_since"].get<std::string>()));
  }
  if (j.contains("sort")) {
    set_sort_mode(j["sort"].get<std::string>());
  }
}

Config Config::from_json(const nlohmann::json &j) {
  Config cfg;
  cfg.load_json(j);
  return cfg;
}

Config Config::from_file(const std::string &path) {
  auto pos = path.find_last_of('.');
  if (pos == std::string::npos) {
    throw std::runtime_error("Unknown config file extension");
  }
  std::string ext = path.substr(pos + 1);
  nlohmann::json j;
  if (ext == "yaml" || ext == "yml") {
    YAML::Node node = YAML::LoadFile(path);
    j = yaml_to_json(node);
  } else if (ext == "json") {
    std::ifstream f(path);
    if (!f) {
      throw std::runtime_error("Failed to open config file");
    }
    f >> j;
  } else {
    throw std::runtime_error("Unsupported config format");
  }
  Config cfg;
  cfg.load_json(j);
  return cfg;
}

} // namespace agpm
