#include "config.hpp"
#include "log.hpp"
#include "util/duration.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <toml++/toml.h>
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
    auto &array = arr.get_ref<json::array_t &>();
    array.reserve(node.size());
    std::transform(node.begin(), node.end(), std::back_inserter(array),
                   [](const YAML::Node &item) { return yaml_to_json(item); });
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

nlohmann::json toml_to_json(const toml::node &node) {
  using nlohmann::json;
  if (const auto *table = node.as_table()) {
    json obj = json::object();
    for (const auto &kv : *table) {
      obj[kv.first.str()] = toml_to_json(kv.second);
    }
    return obj;
  }

  if (const auto *array = node.as_array()) {
    json arr = json::array();
    arr.get_ref<json::array_t &>().reserve(array->size());
    for (const auto &item : *array) {
      arr.push_back(toml_to_json(item));
    }
    return arr;
  }

  if (const auto *value = node.as_boolean())
    return value->get();
  if (const auto *value = node.as_integer())
    return value->get();
  if (const auto *value = node.as_floating_point())
    return value->get();
  if (const auto *value = node.as_string())
    return value->get();

  auto stringify_temporal = [](const auto &temporal) {
    std::ostringstream oss;
    oss << temporal;
    return oss.str();
  };

  if (const auto *value = node.as_date())
    return stringify_temporal(value->get());
  if (const auto *value = node.as_time())
    return stringify_temporal(value->get());
  if (const auto *value = node.as_date_time())
    return stringify_temporal(value->get());

  return nullptr;
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
  if (j.contains("workers")) {
    set_workers(std::max(1, j["workers"].get<int>()));
  }
  if (j.contains("http_timeout")) {
    set_http_timeout(j["http_timeout"].get<int>());
  }
  if (j.contains("http_retries")) {
    set_http_retries(j["http_retries"].get<int>());
  }
  if (j.contains("api_base")) {
    set_api_base(j["api_base"].get<std::string>());
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
  if (j.contains("http_proxy")) {
    set_http_proxy(j["http_proxy"].get<std::string>());
  }
  if (j.contains("https_proxy")) {
    set_https_proxy(j["https_proxy"].get<std::string>());
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
  if (j.contains("protected_branches")) {
    set_protected_branches(
        j["protected_branches"].get<std::vector<std::string>>());
  }
  if (j.contains("protected_branch_excludes")) {
    set_protected_branch_excludes(
        j["protected_branch_excludes"].get<std::vector<std::string>>());
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
  // Merge rule settings
  if (j.contains("required_approvals")) {
    set_required_approvals(j["required_approvals"].get<int>());
  }
  if (j.contains("require_status_success")) {
    set_require_status_success(j["require_status_success"].get<bool>());
  }
  if (j.contains("require_mergeable_state")) {
    set_require_mergeable_state(j["require_mergeable_state"].get<bool>());
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
  if (j.contains("use_graphql")) {
    set_use_graphql(j["use_graphql"].get<bool>());
  }
  if (j.contains("hotkeys_enabled")) {
    set_hotkeys_enabled(j["hotkeys_enabled"].get<bool>());
  }
  if (j.contains("hotkeys")) {
    const auto &hot = j["hotkeys"];
    if (hot.is_boolean()) {
      set_hotkeys_enabled(hot.get<bool>());
    } else if (hot.is_object()) {
      if (hot.contains("enabled") && hot["enabled"].is_boolean()) {
        set_hotkeys_enabled(hot["enabled"].get<bool>());
      }
      if (hot.contains("bindings") && hot["bindings"].is_object()) {
        for (const auto &[action, value] : hot["bindings"].items()) {
          if (value.is_string()) {
            set_hotkey_binding(action, value.get<std::string>());
          } else if (value.is_array()) {
            std::string merged;
            for (const auto &item : value) {
              if (!item.is_string()) {
                continue;
              }
              if (!merged.empty()) {
                merged.push_back(',');
              }
              merged += item.get<std::string>();
            }
            set_hotkey_binding(action, merged);
          } else if (value.is_boolean()) {
            set_hotkey_binding(action, value.get<bool>() ? "default" : "none");
          } else if (value.is_null()) {
            set_hotkey_binding(action, "");
          }
        }
      }
    }
  }

  // Warn on repositories appearing in both include and exclude lists.
  if (!include_repos_.empty() && !exclude_repos_.empty()) {
    std::unordered_set<std::string> exclude(exclude_repos_.begin(),
                                            exclude_repos_.end());
    for (const auto &r : include_repos_) {
      if (exclude.count(r) > 0) {
        spdlog::warn(
            "Repository '{}' listed in both include_repos and exclude_repos;"
            " exclusion takes precedence",
            r);
      }
    }
  }

  // Warn when protected branch patterns are overridden by explicit excludes.
  if (!protected_branches_.empty() && !protected_branch_excludes_.empty()) {
    std::unordered_set<std::string> protected_set(protected_branches_.begin(),
                                                  protected_branches_.end());
    for (const auto &p : protected_branch_excludes_) {
      if (protected_set.count(p) > 0) {
        spdlog::warn(
            "Protected branch pattern '{}' also present in protected_branch_"
            "excludes; exclusion takes precedence",
            p);
      }
    }
  }
}

Config Config::from_json(const nlohmann::json &j) {
  Config cfg;
  cfg.load_json(j);
  return cfg;
}

Config Config::from_file(const std::string &path) {
  ensure_default_logger();
  spdlog::debug("Loading config from {}", path);
  auto pos = path.find_last_of('.');
  if (pos == std::string::npos) {
    spdlog::error("Unknown config file extension for {}", path);
    throw std::runtime_error("Unknown config file extension");
  }
  std::string ext = path.substr(pos + 1);
  std::string ext_lower = ext;
  std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  spdlog::debug("Detected config file type: {}", ext_lower);
  nlohmann::json j;
  try {
    if (ext_lower == "yaml" || ext_lower == "yml") {
      YAML::Node node = YAML::LoadFile(path);
      j = yaml_to_json(node);
    } else if (ext_lower == "json") {
      std::ifstream f(path);
      if (!f) {
        spdlog::error("Failed to open config file {}", path);
        throw std::runtime_error("Failed to open config file");
      }
      f >> j;
    } else if (ext_lower == "toml" || ext_lower == "tml") {
      toml::table tbl = toml::parse_file(path);
      j = toml_to_json(tbl);
    } else {
      spdlog::error("Unsupported config format: {}", ext);
      throw std::runtime_error("Unsupported config format");
    }
  } catch (const std::exception &e) {
    spdlog::error("Failed to load config {}: {}", path, e.what());
    throw;
  }
  Config cfg;
  cfg.load_json(j);
  spdlog::info("Config loaded successfully from {}", path);
  return cfg;
}

} // namespace agpm
