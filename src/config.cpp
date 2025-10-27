#include "config.hpp"
#include "log.hpp"
#include "token_loader.hpp"
#include "util/duration.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <toml++/toml.h>
#include <yaml-cpp/yaml.h>

namespace agpm {

namespace {

std::shared_ptr<spdlog::logger> config_log() {
  static auto logger = [] {
    ensure_default_logger();
    return category_logger("config");
  }();
  return logger;
}

std::string to_lower_copy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

/**
 * Convert a YAML node into a structurally equivalent JSON object.
 *
 * The conversion preserves scalar types where possible and recursively maps
 * sequences and maps to JSON arrays and objects respectively.
 *
 * @param node YAML node to transform.
 * @return JSON value mirroring the YAML content.
 */
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

/**
 * Translate a TOML node to a JSON representation.
 *
 * @param node TOML node read from a parsed document.
 * @return JSON value containing the equivalent data.
 */
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

/**
 * Merge recognised configuration sections into the root object so grouped
 * configuration files can expose the same flat keys that the loader expects.
 *
 * This preserves backwards compatibility with legacy flat configurations while
 * allowing new examples to organise related settings under nested objects such
 * as `core`, `rate_limits`, or `logging`.
 */
nlohmann::json normalize_config_sections(const nlohmann::json &source) {
  nlohmann::json normalized = source;
  auto merge_section = [&normalized](std::string_view name) {
    auto it = normalized.find(std::string{name});
    if (it == normalized.end() || !it->is_object()) {
      return;
    }
    const nlohmann::json section = *it;
    for (const auto &[key, value] : section.items()) {
      normalized[key] = value;
    }
  };

  for (std::string_view section :
       {"core", "rate_limits", "logging", "network", "repositories", "tokens",
        "features", "integrations", "workflow", "artifacts", "ui",
        "personal_access_tokens", "pat", "single_run", "mcp"}) {
    merge_section(section);
  }

  return normalized;
}

std::regex repo_glob_to_regex(const std::string &glob) {
  std::string rx = "^";
  for (char c : glob) {
    switch (c) {
    case '*':
      rx += ".*";
      break;
    case '?':
      rx += '.';
      break;
    case '.':
    case '+':
    case '(':
    case ')':
    case '{':
    case '}':
    case '^':
    case '$':
    case '|':
    case '\\':
    case '[':
    case ']':
      rx += '\\';
      rx += c;
      break;
    default:
      rx += c;
    }
  }
  rx += '$';
  return std::regex(rx);
}

std::string repo_mixed_to_regex(const std::string &value) {
  std::string out;
  out.reserve(value.size() * 2);
  for (char c : value) {
    switch (c) {
    case '*':
      out += ".*";
      break;
    case '?':
      out += '.';
      break;
    default:
      out.push_back(c);
    }
  }
  return out;
}

std::optional<std::regex> compile_repo_pattern(const std::string &pattern) {
  std::string body = pattern;
  if (body.rfind("regex:", 0) == 0) {
    body.erase(0, std::string("regex:").size());
    try {
      return std::regex(body);
    } catch (const std::exception &e) {
      config_log()->warn("Invalid regex repository override '{}': {}", pattern,
                         e.what());
      return std::nullopt;
    }
  }
  if (body.rfind("glob:", 0) == 0 || body.rfind("wildcard:", 0) == 0) {
    auto pos = body.find(':');
    body.erase(0, pos + 1);
    return repo_glob_to_regex(body);
  }
  if (body.rfind("mixed:", 0) == 0) {
    body.erase(0, std::string("mixed:").size());
    std::string mixed = repo_mixed_to_regex(body);
    try {
      return std::regex(mixed);
    } catch (const std::exception &e) {
      config_log()->warn("Invalid mixed pattern '{}': {}", pattern, e.what());
      return std::nullopt;
    }
  }
  if (body.find('*') != std::string::npos || body.find('?') != std::string::npos) {
    return repo_glob_to_regex(body);
  }
  return std::nullopt;
}

std::vector<std::pair<std::string, std::string>>
parse_hook_headers(const nlohmann::json &headers, std::string_view context) {
  std::vector<std::pair<std::string, std::string>> parsed;
  if (!headers.is_object()) {
    config_log()->warn("Hook headers for '{}' must be an object", context);
    return parsed;
  }
  for (const auto &[key, value] : headers.items()) {
    if (!value.is_string()) {
      config_log()->warn(
          "Hook header '{}' for '{}' must be a string value", key, context);
      continue;
    }
    parsed.emplace_back(key, value.get<std::string>());
  }
  return parsed;
}

std::optional<HookAction> parse_hook_action(const nlohmann::json &value,
                                            std::string_view context) {
  if (!value.is_object()) {
    config_log()->warn("Hook action for '{}' must be an object", context);
    return std::nullopt;
  }
  HookAction action;
  std::string type;
  if (value.contains("type") && value["type"].is_string()) {
    type = to_lower_copy(value["type"].get<std::string>());
  } else if (value.contains("command")) {
    type = "command";
  } else if (value.contains("endpoint")) {
    type = "http";
  } else {
    config_log()->warn("Hook action for '{}' missing type/command/endpoint",
                       context);
    return std::nullopt;
  }
  if (type == "command") {
    if (!value.contains("command") || !value["command"].is_string()) {
      config_log()->warn("Command hook action for '{}' missing command",
                         context);
      return std::nullopt;
    }
    action.type = HookActionType::Command;
    action.command = value["command"].get<std::string>();
  } else if (type == "http" || type == "endpoint") {
    if (!value.contains("endpoint") || !value["endpoint"].is_string()) {
      config_log()->warn("HTTP hook action for '{}' missing endpoint",
                         context);
      return std::nullopt;
    }
    action.type = HookActionType::Http;
    action.endpoint = value["endpoint"].get<std::string>();
    if (value.contains("method") && value["method"].is_string()) {
      action.method = value["method"].get<std::string>();
    }
    if (value.contains("headers")) {
      action.headers = parse_hook_headers(value["headers"], context);
    }
  } else {
    config_log()->warn("Unsupported hook action type '{}' for '{}'", type,
                       context);
    return std::nullopt;
  }
  return action;
}

void parse_repository_actions(const nlohmann::json &node,
                              Config::RepositoryActionOverride &out) {
  auto assign_bool = [&](std::string_view key, bool &has_value, bool &value) {
    auto it = node.find(std::string(key));
    if (it != node.end() && it->is_boolean()) {
      has_value = true;
      value = it->get<bool>();
    }
  };
  assign_bool("only_poll_prs", out.has_only_poll_prs, out.only_poll_prs);
  assign_bool("only_poll_stray", out.has_only_poll_stray, out.only_poll_stray);
  assign_bool("purge_only", out.has_purge_only, out.purge_only);
  assign_bool("auto_merge", out.has_auto_merge, out.auto_merge);
  assign_bool("reject_dirty", out.has_reject_dirty, out.reject_dirty);
  assign_bool("delete_stray", out.has_delete_stray, out.delete_stray);
  assign_bool("hooks_enabled", out.has_hooks_enabled, out.hooks_enabled);
  auto prefix_it = node.find("purge_prefix");
  if (prefix_it != node.end() && prefix_it->is_string()) {
    out.has_purge_prefix = true;
    out.purge_prefix = prefix_it->get<std::string>();
  }
  auto nested_it = node.find("actions");
  if (nested_it != node.end() && nested_it->is_object()) {
    parse_repository_actions(*nested_it, out);
  }
}

void parse_repository_hooks(const nlohmann::json &node,
                            Config::RepositoryHookOverride &out,
                            std::string_view context) {
  auto parse_default_command = [&](const nlohmann::json &source,
                                   std::string_view field) {
    auto it = source.find(std::string(field));
    if (it != source.end() && it->is_string()) {
      HookAction action;
      action.type = HookActionType::Command;
      action.command = it->get<std::string>();
      out.overrides_default_actions = true;
      out.default_actions.push_back(std::move(action));
    }
  };
  auto parse_default_http = [&](const nlohmann::json &source,
                                std::string_view endpoint_field,
                                std::string_view method_field,
                                std::string_view headers_field) {
    auto it = source.find(std::string(endpoint_field));
    if (it == source.end() || !it->is_string()) {
      return;
    }
    HookAction action;
    action.type = HookActionType::Http;
    action.endpoint = it->get<std::string>();
    auto method_it = source.find(std::string(method_field));
    if (method_it != source.end() && method_it->is_string()) {
      action.method = method_it->get<std::string>();
    }
    auto headers_it = source.find(std::string(headers_field));
    if (headers_it != source.end()) {
      action.headers = parse_hook_headers(*headers_it, context);
    }
    out.overrides_default_actions = true;
    out.default_actions.push_back(std::move(action));
  };
  auto parse_actions_array = [&](const nlohmann::json &arr,
                                 std::vector<HookAction> &dest) {
    for (const auto &entry : arr) {
      if (auto action = parse_hook_action(entry, context)) {
        dest.push_back(*action);
      }
    }
  };

  if (node.contains("hooks_enabled") && node["hooks_enabled"].is_boolean()) {
    out.has_enabled = true;
    out.enabled = node["hooks_enabled"].get<bool>();
  }
  parse_default_command(node, "hook_command");
  parse_default_http(node, "hook_endpoint", "hook_method", "hook_headers");
  auto top_actions = node.find("hook_actions");
  if (top_actions != node.end() && top_actions->is_array()) {
    out.overrides_default_actions = true;
    parse_actions_array(*top_actions, out.default_actions);
  }
  auto top_events = node.find("hook_event_actions");
  if (top_events != node.end() && top_events->is_object()) {
    out.overrides_event_actions = true;
    for (const auto &[event_name, actions] : top_events->items()) {
      if (!actions.is_array()) {
        config_log()->warn(
            "hook_event_actions entry '{}' for '{}' must be an array",
            event_name, context);
        continue;
      }
      std::vector<HookAction> parsed;
      parse_actions_array(actions, parsed);
      out.event_actions[event_name] = std::move(parsed);
    }
  }
  auto hooks_it = node.find("hooks");
  if (hooks_it != node.end() && hooks_it->is_object()) {
    const auto &hooks = *hooks_it;
    if (hooks.contains("enabled") && hooks["enabled"].is_boolean()) {
      out.has_enabled = true;
      out.enabled = hooks["enabled"].get<bool>();
    }
    parse_default_command(hooks, "command");
    parse_default_http(hooks, "endpoint", "method", "headers");
    auto hooks_actions = hooks.find("actions");
    if (hooks_actions != hooks.end() && hooks_actions->is_array()) {
      out.overrides_default_actions = true;
      parse_actions_array(*hooks_actions, out.default_actions);
    }
    auto hooks_events = hooks.find("event_actions");
    if (hooks_events != hooks.end() && hooks_events->is_object()) {
      out.overrides_event_actions = true;
      for (const auto &[event_name, actions] : hooks_events->items()) {
        if (!actions.is_array()) {
          config_log()->warn(
              "hooks.event_actions entry '{}' for '{}' must be an array",
              event_name, context);
          continue;
        }
        std::vector<HookAction> parsed;
        parse_actions_array(actions, parsed);
        out.event_actions[event_name] = std::move(parsed);
      }
    }
  }
}

} // namespace

void Config::set_rate_limit_margin(double margin) {
  rate_limit_margin_ = std::clamp(margin, 0.0, 0.95);
}

/**
 * Populate configuration settings from a JSON object.
 *
 * @param j JSON document holding configuration keys.
 * @throws nlohmann::json::exception When required values cannot be converted
 *         to the expected types.
 */
void Config::load_json(const nlohmann::json &j) {
  nlohmann::json cfg = normalize_config_sections(j);

  if (cfg.contains("verbose")) {
    set_verbose(cfg["verbose"].get<bool>());
  }
  if (cfg.contains("poll_interval")) {
    set_poll_interval(cfg["poll_interval"].get<int>());
  }
  if (cfg.contains("max_request_rate")) {
    set_max_request_rate(cfg["max_request_rate"].get<int>());
  }
  if (cfg.contains("max_hourly_requests")) {
    set_max_hourly_requests(cfg["max_hourly_requests"].get<int>());
  }
  if (cfg.contains("rate_limit_margin")) {
    set_rate_limit_margin(cfg["rate_limit_margin"].get<double>());
  }
  if (cfg.contains("rate_limit_refresh_interval")) {
    set_rate_limit_refresh_interval(
        cfg["rate_limit_refresh_interval"].get<int>());
  }
  if (cfg.contains("retry_rate_limit_endpoint")) {
    set_retry_rate_limit_endpoint(cfg["retry_rate_limit_endpoint"].get<bool>());
  }
  if (cfg.contains("rate_limit_retry_limit")) {
    set_rate_limit_retry_limit(cfg["rate_limit_retry_limit"].get<int>());
  }
  if (cfg.contains("workers")) {
    set_workers(std::max(1, cfg["workers"].get<int>()));
  }
  if (cfg.contains("http_timeout")) {
    set_http_timeout(cfg["http_timeout"].get<int>());
  }
  if (cfg.contains("http_retries")) {
    set_http_retries(cfg["http_retries"].get<int>());
  }
  if (cfg.contains("api_base")) {
    set_api_base(cfg["api_base"].get<std::string>());
  }
  if (cfg.contains("download_limit")) {
    set_download_limit(cfg["download_limit"].get<long long>());
  }
  if (cfg.contains("upload_limit")) {
    set_upload_limit(cfg["upload_limit"].get<long long>());
  }
  if (cfg.contains("max_download")) {
    set_max_download(cfg["max_download"].get<long long>());
  }
  if (cfg.contains("max_upload")) {
    set_max_upload(cfg["max_upload"].get<long long>());
  }
  if (cfg.contains("http_proxy")) {
    set_http_proxy(cfg["http_proxy"].get<std::string>());
  }
  if (cfg.contains("https_proxy")) {
    set_https_proxy(cfg["https_proxy"].get<std::string>());
  }
  if (cfg.contains("log_level")) {
    set_log_level(cfg["log_level"].get<std::string>());
  }
  if (cfg.contains("log_pattern")) {
    set_log_pattern(cfg["log_pattern"].get<std::string>());
  }
  if (cfg.contains("log_file")) {
    set_log_file(cfg["log_file"].get<std::string>());
  }
  if (cfg.contains("log_limit")) {
    set_log_limit(cfg["log_limit"].get<int>());
  }
  if (cfg.contains("log_rotate")) {
    set_log_rotate(cfg["log_rotate"].get<int>());
  }
  if (cfg.contains("log_compress")) {
    set_log_compress(cfg["log_compress"].get<bool>());
  }
  if (cfg.contains("log_sidecar")) {
    set_log_sidecar(cfg["log_sidecar"].get<bool>());
  }
  if (cfg.contains("request_caddy_window")) {
    set_request_caddy_window(cfg["request_caddy_window"].get<bool>());
  }
  if (cfg.contains("log_categories")) {
    std::unordered_map<std::string, std::string> categories;
    const auto &value = cfg["log_categories"];
    auto assign_category = [&categories](std::string name, std::string level) {
      if (name.empty()) {
        return;
      }
      if (level.empty()) {
        level = "debug";
      }
      categories[std::move(name)] = std::move(level);
    };
    if (value.is_object()) {
      for (const auto &[key, v] : value.items()) {
        if (v.is_string()) {
          assign_category(key, v.get<std::string>());
        } else if (v.is_null()) {
          assign_category(key, "debug");
        } else {
          config_log()->warn("Unsupported value for log category '{}'; "
                             "expected string or null",
                             key);
        }
      }
    } else if (value.is_array()) {
      for (const auto &item : value) {
        if (!item.is_string()) {
          continue;
        }
        std::string raw = item.get<std::string>();
        auto pos = raw.find('=');
        assign_category(pos == std::string::npos ? raw : raw.substr(0, pos),
                        pos == std::string::npos ? std::string{"debug"}
                                                 : raw.substr(pos + 1));
      }
    } else if (value.is_string()) {
      std::string raw = value.get<std::string>();
      auto pos = raw.find('=');
      assign_category(pos == std::string::npos ? raw : raw.substr(0, pos),
                      pos == std::string::npos ? std::string{"debug"}
                                               : raw.substr(pos + 1));
    }
    set_log_categories(std::move(categories));
  }
  if (cfg.contains("include_repos")) {
    set_include_repos(cfg["include_repos"].get<std::vector<std::string>>());
  }
  if (cfg.contains("exclude_repos")) {
    set_exclude_repos(cfg["exclude_repos"].get<std::vector<std::string>>());
  }
  if (cfg.contains("protected_branches")) {
    set_protected_branches(
        cfg["protected_branches"].get<std::vector<std::string>>());
  }
  if (cfg.contains("protected_branch_excludes")) {
    set_protected_branch_excludes(
        cfg["protected_branch_excludes"].get<std::vector<std::string>>());
  }
  if (cfg.contains("include_merged")) {
    set_include_merged(cfg["include_merged"].get<bool>());
  }
  if (cfg.contains("repo_discovery_mode")) {
    try {
      set_repo_discovery_mode(repo_discovery_mode_from_string(
          cfg["repo_discovery_mode"].get<std::string>()));
    } catch (const std::exception &e) {
      config_log()->error("Invalid repo_discovery_mode: {}", e.what());
      throw;
    }
  }
  if (cfg.contains("repo_discovery_roots")) {
    set_repo_discovery_roots(
        cfg["repo_discovery_roots"].get<std::vector<std::string>>());
  }
  if (cfg.contains("repo_discovery_root")) {
    add_repo_discovery_root(cfg["repo_discovery_root"].get<std::string>());
  }
  if (cfg.contains("api_keys")) {
    set_api_keys(cfg["api_keys"].get<std::vector<std::string>>());
  }
  if (cfg.contains("api_key_from_stream")) {
    set_api_key_from_stream(cfg["api_key_from_stream"].get<bool>());
  }
  if (cfg.contains("api_key_url")) {
    set_api_key_url(cfg["api_key_url"].get<std::string>());
  }
  if (cfg.contains("api_key_url_user")) {
    set_api_key_url_user(cfg["api_key_url_user"].get<std::string>());
  }
  if (cfg.contains("api_key_url_password")) {
    set_api_key_url_password(cfg["api_key_url_password"].get<std::string>());
  }
  if (cfg.contains("api_key_files")) {
    set_api_key_files(cfg["api_key_files"].get<std::vector<std::string>>());
  }
  if (cfg.contains("api_key_file")) {
    add_api_key_file(cfg["api_key_file"].get<std::string>());
  }
  if (cfg.contains("history_db")) {
    set_history_db(cfg["history_db"].get<std::string>());
  }
  if (!api_key_files().empty()) {
    for (const auto &file : api_key_files()) {
      try {
        auto file_tokens = load_tokens_from_file(file);
        api_keys_.insert(api_keys_.end(), file_tokens.begin(),
                         file_tokens.end());
      } catch (const std::exception &e) {
        config_log()->error("Failed to load token file {}: {}", file, e.what());
        throw;
      }
    }
  }
  if (cfg.contains("export_csv")) {
    set_export_csv(cfg["export_csv"].get<std::string>());
  }
  if (cfg.contains("export_json")) {
    set_export_json(cfg["export_json"].get<std::string>());
  }
  if (cfg.contains("assume_yes")) {
    set_assume_yes(cfg["assume_yes"].get<bool>());
  }
  if (cfg.contains("dry_run")) {
    set_dry_run(cfg["dry_run"].get<bool>());
  }
  if (cfg.contains("only_poll_prs")) {
    set_only_poll_prs(cfg["only_poll_prs"].get<bool>());
  }
  if (cfg.contains("only_poll_stray")) {
    set_only_poll_stray(cfg["only_poll_stray"].get<bool>());
  }
  if (cfg.contains("purge_only")) {
    set_purge_only(cfg["purge_only"].get<bool>());
  }
  if (cfg.contains("reject_dirty")) {
    set_reject_dirty(cfg["reject_dirty"].get<bool>());
  }
  if (cfg.contains("delete_stray")) {
    set_delete_stray(cfg["delete_stray"].get<bool>());
  }
  if (cfg.contains("heuristic_stray_detection")) {
    set_heuristic_stray_detection(cfg["heuristic_stray_detection"].get<bool>());
  }
  if (cfg.contains("stray_detection_engine")) {
    const auto &engine = cfg["stray_detection_engine"];
    if (!engine.is_string()) {
      config_log()->error(
          "Expected stray_detection_engine to be a string but received {}",
          engine.type_name());
      throw std::runtime_error(
          "Invalid stray_detection_engine value in configuration");
    }
    auto mode = stray_detection_mode_from_string(engine.get<std::string>());
    if (!mode) {
      config_log()->error(
          "Unrecognised stray_detection_engine value '{}'. Valid options are "
          "rule, heuristic, both",
          engine.get<std::string>());
      throw std::runtime_error(
          "Invalid stray_detection_engine value in configuration");
    }
    set_stray_detection_mode(*mode);
  }
  if (cfg.contains("auto_merge")) {
    set_auto_merge(cfg["auto_merge"].get<bool>());
  }
  if (cfg.contains("allow_delete_base_branch")) {
    set_allow_delete_base_branch(cfg["allow_delete_base_branch"].get<bool>());
  }
  // Merge rule settings
  if (cfg.contains("required_approvals")) {
    set_required_approvals(cfg["required_approvals"].get<int>());
  }
  if (cfg.contains("require_status_success")) {
    set_require_status_success(cfg["require_status_success"].get<bool>());
  }
  if (cfg.contains("require_mergeable_state")) {
    set_require_mergeable_state(cfg["require_mergeable_state"].get<bool>());
  }
  if (cfg.contains("purge_prefix")) {
    set_purge_prefix(cfg["purge_prefix"].get<std::string>());
  }
  if (cfg.contains("pr_limit")) {
    set_pr_limit(cfg["pr_limit"].get<int>());
  }
  if (cfg.contains("pr_since")) {
    set_pr_since(parse_duration(cfg["pr_since"].get<std::string>()));
  }
  if (cfg.contains("sort")) {
    set_sort_mode(cfg["sort"].get<std::string>());
  }
  if (cfg.contains("use_graphql")) {
    set_use_graphql(cfg["use_graphql"].get<bool>());
  }
  if (cfg.contains("hotkeys_enabled")) {
    set_hotkeys_enabled(cfg["hotkeys_enabled"].get<bool>());
  }
  if (cfg.contains("hotkeys")) {
    const auto &hot = cfg["hotkeys"];
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
  if (cfg.contains("open_pat_page")) {
    set_open_pat_page(cfg["open_pat_page"].get<bool>());
  }
  if (cfg.contains("pat_save_path")) {
    set_pat_save_path(cfg["pat_save_path"].get<std::string>());
  }
  if (cfg.contains("pat_value")) {
    set_pat_value(cfg["pat_value"].get<std::string>());
  }
  if (cfg.contains("single_open_prs_repo")) {
    set_single_open_prs_repo(cfg["single_open_prs_repo"].get<std::string>());
  }
  if (cfg.contains("single_branches_repo")) {
    set_single_branches_repo(cfg["single_branches_repo"].get<std::string>());
  }
  if (cfg.contains("hooks") && cfg["hooks"].is_object()) {
    const auto &hooks = cfg["hooks"];
    if (hooks.contains("enabled") && hooks["enabled"].is_boolean()) {
      set_hooks_enabled(hooks["enabled"].get<bool>());
    }
    if (hooks.contains("command") && hooks["command"].is_string()) {
      set_hook_command(hooks["command"].get<std::string>());
    }
    if (hooks.contains("endpoint") && hooks["endpoint"].is_string()) {
      set_hook_endpoint(hooks["endpoint"].get<std::string>());
    }
    if (hooks.contains("method") && hooks["method"].is_string()) {
      set_hook_method(hooks["method"].get<std::string>());
    }
    if (hooks.contains("headers") && hooks["headers"].is_object()) {
      std::unordered_map<std::string, std::string> headers;
      for (const auto &[key, value] : hooks["headers"].items()) {
        if (value.is_string()) {
          headers[key] = value.get<std::string>();
        }
      }
      set_hook_headers(std::move(headers));
    }
    if (hooks.contains("pull_threshold") &&
        hooks["pull_threshold"].is_number()) {
      set_hook_pull_threshold(hooks["pull_threshold"].get<int>());
    }
    if (hooks.contains("branch_threshold") &&
        hooks["branch_threshold"].is_number()) {
      set_hook_branch_threshold(hooks["branch_threshold"].get<int>());
    }
  }
  if (cfg.contains("hooks_enabled")) {
    set_hooks_enabled(cfg["hooks_enabled"].get<bool>());
  }
  if (cfg.contains("hooks_command")) {
    set_hook_command(cfg["hooks_command"].get<std::string>());
  }
  if (cfg.contains("hooks_endpoint")) {
    set_hook_endpoint(cfg["hooks_endpoint"].get<std::string>());
  }
  if (cfg.contains("hooks_method")) {
    set_hook_method(cfg["hooks_method"].get<std::string>());
  }
  if (cfg.contains("hooks_headers") && cfg["hooks_headers"].is_object()) {
    std::unordered_map<std::string, std::string> headers;
    for (const auto &[key, value] : cfg["hooks_headers"].items()) {
      if (value.is_string()) {
        headers[key] = value.get<std::string>();
      }
    }
    set_hook_headers(std::move(headers));
  }
  if (cfg.contains("hooks_pull_threshold")) {
    set_hook_pull_threshold(cfg["hooks_pull_threshold"].get<int>());
  }
  if (cfg.contains("hooks_branch_threshold")) {
    set_hook_branch_threshold(cfg["hooks_branch_threshold"].get<int>());
  }
  repository_overrides_.clear();
  if (cfg.contains("repository_overrides")) {
    const auto &overrides = cfg["repository_overrides"];
    auto add_override = [&](const std::string &pattern,
                            const nlohmann::json &payload) {
      if (!payload.is_object()) {
        config_log()->warn(
            "Repository override '{}' ignored because value is not an object",
            pattern);
        return;
      }
      RepositoryOverride entry;
      entry.pattern = pattern;
      entry.compiled_pattern = compile_repo_pattern(pattern);
      parse_repository_actions(payload, entry.actions);
      parse_repository_hooks(payload, entry.hooks, pattern);
      repository_overrides_.push_back(std::move(entry));
    };
    if (overrides.is_object()) {
      for (const auto &[pattern, payload] : overrides.items()) {
        add_override(pattern, payload);
      }
    } else if (overrides.is_array()) {
      for (const auto &item : overrides) {
        if (!item.is_object()) {
          config_log()->warn(
              "repository_overrides array entries must be objects; skipping");
          continue;
        }
        auto pat_it = item.find("pattern");
        if (pat_it == item.end() || !pat_it->is_string()) {
          config_log()->warn(
              "repository_overrides array entry missing string 'pattern'; "
              "skipping");
          continue;
        }
        std::string pattern = pat_it->get<std::string>();
        add_override(pattern, item);
      }
    } else {
      config_log()->warn(
          "repository_overrides must be an object mapping patterns or an array"
          " of override objects");
    }
  }
  if (cfg.contains("mcp_server_enabled")) {
    set_mcp_server_enabled(cfg["mcp_server_enabled"].get<bool>());
  }
  if (cfg.contains("mcp_server_bind_address")) {
    set_mcp_server_bind_address(
        cfg["mcp_server_bind_address"].get<std::string>());
  }
  if (cfg.contains("mcp_server_port")) {
    set_mcp_server_port(cfg["mcp_server_port"].get<int>());
  }
  if (cfg.contains("mcp_server_backlog")) {
    set_mcp_server_backlog(cfg["mcp_server_backlog"].get<int>());
  }
  if (cfg.contains("mcp_server_max_clients")) {
    set_mcp_server_max_clients(
        cfg["mcp_server_max_clients"].get<int>());
  }
  if (cfg.contains("mcp_server_caddy_window")) {
    set_mcp_server_caddy_window(
        cfg["mcp_server_caddy_window"].get<bool>());
  }
  if (cfg.contains("mcp")) {
    const auto &mcp_cfg = cfg["mcp"];
    if (mcp_cfg.is_object()) {
      if (mcp_cfg.contains("enabled")) {
        set_mcp_server_enabled(mcp_cfg["enabled"].get<bool>());
      }
      if (mcp_cfg.contains("bind") && mcp_cfg["bind"].is_string()) {
        set_mcp_server_bind_address(mcp_cfg["bind"].get<std::string>());
      }
      if (mcp_cfg.contains("bind_address") &&
          mcp_cfg["bind_address"].is_string()) {
        set_mcp_server_bind_address(
            mcp_cfg["bind_address"].get<std::string>());
      }
      if (mcp_cfg.contains("port")) {
        set_mcp_server_port(mcp_cfg["port"].get<int>());
      }
      if (mcp_cfg.contains("backlog")) {
        set_mcp_server_backlog(mcp_cfg["backlog"].get<int>());
      }
      if (mcp_cfg.contains("max_clients")) {
        set_mcp_server_max_clients(mcp_cfg["max_clients"].get<int>());
      }
      if (mcp_cfg.contains("caddy_window")) {
        set_mcp_server_caddy_window(mcp_cfg["caddy_window"].get<bool>());
      }
    } else {
      config_log()->warn("Ignoring 'mcp' configuration; expected an object");
    }
  }

  // Warn on repositories appearing in both include and exclude lists.
  if (!include_repos_.empty() && !exclude_repos_.empty()) {
    std::unordered_set<std::string> exclude(exclude_repos_.begin(),
                                            exclude_repos_.end());
    for (const auto &r : include_repos_) {
      if (exclude.count(r) > 0) {
        config_log()->warn(
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
        config_log()->warn(
            "Protected branch pattern '{}' also present in protected_branch_"
            "excludes; exclusion takes precedence",
            p);
      }
    }
  }
}

const Config::RepositoryOverride *
Config::match_repository_override(const std::string &owner,
                                  const std::string &repo) const {
  if (repository_overrides_.empty()) {
    return nullptr;
  }
  std::string key = owner + "/" + repo;
  for (const auto &entry : repository_overrides_) {
    if (entry.compiled_pattern) {
      try {
        if (std::regex_match(key, *entry.compiled_pattern)) {
          return &entry;
        }
      } catch (const std::exception &e) {
        config_log()->warn("Regex match failed for override '{}': {}",
                           entry.pattern, e.what());
      }
    } else if (entry.pattern == key) {
      return &entry;
    }
  }
  return nullptr;
}

/**
 * Construct a configuration object from a JSON representation.
 *
 * @param j JSON document with configuration values.
 * @return Populated configuration instance.
 * @throws nlohmann::json::exception When value conversions fail.
 */
Config Config::from_json(const nlohmann::json &j) {
  Config cfg;
  cfg.load_json(j);
  return cfg;
}

/**
 * Load configuration from a file on disk.
 *
 * The file type is inferred from the extension and may be YAML, JSON, or
 * TOML. Errors during parsing are logged and rethrown.
 *
 * @param path Filesystem location of the configuration file.
 * @return Fully populated configuration object.
 * @throws std::runtime_error When the file cannot be opened, parsed, or when
 *         the extension is unsupported.
 */
Config Config::from_file(const std::string &path) {
  ensure_default_logger();
  config_log()->debug("Loading config from {}", path);
  auto pos = path.find_last_of('.');
  if (pos == std::string::npos) {
    config_log()->error("Unknown config file extension for {}", path);
    throw std::runtime_error("Unknown config file extension");
  }
  std::string ext = path.substr(pos + 1);
  std::string ext_lower = ext;
  std::transform(
      ext_lower.begin(), ext_lower.end(), ext_lower.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  config_log()->debug("Detected config file type: {}", ext_lower);
  nlohmann::json j;
  try {
    if (ext_lower == "yaml" || ext_lower == "yml") {
      YAML::Node node = YAML::LoadFile(path);
      j = yaml_to_json(node);
    } else if (ext_lower == "json") {
      std::ifstream f(path);
      if (!f) {
        config_log()->error("Failed to open config file {}", path);
        throw std::runtime_error("Failed to open config file");
      }
      f >> j;
    } else if (ext_lower == "toml" || ext_lower == "tml") {
      toml::table tbl = toml::parse_file(path);
      j = toml_to_json(tbl);
    } else {
      config_log()->error("Unsupported config format: {}", ext);
      throw std::runtime_error("Unsupported config format");
    }
  } catch (const std::exception &e) {
    config_log()->error("Failed to load config {}: {}", path, e.what());
    throw;
  }
  Config cfg;
  cfg.load_json(j);
  config_log()->info("Config loaded successfully from {}", path);
  return cfg;
}

} // namespace agpm
