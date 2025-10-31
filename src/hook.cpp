#include "hook.hpp"
#include "log.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <curl/curl.h>
#include <iomanip>
#include <ctime>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace agpm {

namespace {

std::shared_ptr<spdlog::logger> hook_log() {
  static auto logger = [] {
    ensure_default_logger();
    return category_logger("hooks");
  }();
  return logger;
}

std::string parameter_env_name(const std::string &name) {
  std::string upper;
  upper.reserve(name.size());
  for (unsigned char ch : name) {
    if (std::isalnum(ch)) {
      upper.push_back(static_cast<char>(std::toupper(ch)));
    } else {
      upper.push_back('_');
    }
  }
  if (upper.empty()) {
    upper = "PARAM";
  }
  return "AGPM_HOOK_PARAM_" + upper;
}

std::string iso_timestamp(std::chrono::system_clock::time_point tp) {
  auto tt = std::chrono::system_clock::to_time_t(tp);
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &tt);
#else
  gmtime_r(&tt, &tm);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

#ifdef _WIN32
void set_env(const std::string &name, const std::string &value) {
  _putenv_s(name.c_str(), value.c_str());
}

void unset_env(const std::string &name) {
  _putenv_s(name.c_str(), "");
}
#else
void set_env(const std::string &name, const std::string &value) {
  setenv(name.c_str(), value.c_str(), 1);
}

void unset_env(const std::string &name) { unsetenv(name.c_str()); }
#endif

class ScopedEnvVar {
public:
  ScopedEnvVar(std::string name, std::string value)
      : name_(std::move(name)), active_(true) {
    const char *prev = std::getenv(name_.c_str());
    if (prev != nullptr) {
      had_previous_ = true;
      previous_ = prev;
    }
    set_env(name_, value);
  }

  ~ScopedEnvVar() {
    if (!active_) {
      return;
    }
    if (had_previous_) {
      set_env(name_, previous_);
    } else {
      unset_env(name_);
    }
  }

  ScopedEnvVar(const ScopedEnvVar &) = delete;
  ScopedEnvVar &operator=(const ScopedEnvVar &) = delete;

private:
  std::string name_;
  std::string previous_;
  bool had_previous_{false};
  bool active_{true};
};

} // namespace

HookDispatcher::HookDispatcher(HookSettings settings,
                               CommandExecutor command_executor,
                               HttpExecutor http_executor)
    : settings_(std::move(settings)),
      command_executor_(std::move(command_executor)),
      http_executor_(std::move(http_executor)) {
  repo_overrides_ = std::move(settings_.repository_overrides);
  if (!settings_.enabled || !has_actions()) {
    if (settings_.enabled && !has_actions()) {
      hook_log()->warn("Hook dispatcher enabled without any configured actions");
    }
    return;
  }
  running_ = true;
  thread_ = std::thread([this] { worker(); });
}

HookDispatcher::~HookDispatcher() {
  {
    std::lock_guard<std::mutex> lk(mutex_);
    stop_ = true;
  }
  cv_.notify_all();
  if (thread_.joinable()) {
    thread_.join();
  }
  running_ = false;
}

void HookDispatcher::enqueue(HookEvent event) {
  if (!running_) {
    return;
  }
  {
    std::lock_guard<std::mutex> lk(mutex_);
    queue_.push_back(std::move(event));
  }
  cv_.notify_one();
}

void HookDispatcher::worker() {
  while (true) {
    HookEvent event;
    {
      std::unique_lock<std::mutex> lk(mutex_);
      cv_.wait(lk, [this] { return stop_ || !queue_.empty(); });
      if (stop_ && queue_.empty()) {
        break;
      }
      event = std::move(queue_.front());
      queue_.pop_front();
    }
    try {
      dispatch(event);
    } catch (const std::exception &e) {
      hook_log()->error("Hook dispatch failed: {}", e.what());
    } catch (...) {
      hook_log()->error("Hook dispatch failed with unknown error");
    }
  }
}

void HookDispatcher::dispatch(const HookEvent &event) {
  auto payload = nlohmann::json{{"event", event.name},
                                {"timestamp", iso_timestamp(
                                                   std::chrono::system_clock::now())},
                                {"data", event.data}};
  const RepositoryHookSettings *override_settings =
      match_repository_override(event);
  bool enabled = settings_.enabled;
  const std::vector<HookAction> *default_actions = &settings_.default_actions;
  if (override_settings) {
    if (override_settings->has_enabled) {
      enabled = override_settings->enabled;
    }
    if (override_settings->overrides_default_actions) {
      default_actions = &override_settings->default_actions;
    }
  }
  if (!enabled) {
    hook_log()->debug("Hooks disabled for event '{}'", event.name);
    return;
  }
  const std::vector<HookAction> *actions_ptr = nullptr;
  if (override_settings && override_settings->overrides_event_actions) {
    auto ov_it = override_settings->event_actions.find(event.name);
    if (ov_it != override_settings->event_actions.end()) {
      actions_ptr = &ov_it->second;
    }
  }
  if (!actions_ptr) {
    auto it = settings_.event_actions.find(event.name);
    if (it != settings_.event_actions.end()) {
      actions_ptr = &it->second;
    }
  }
  if (!actions_ptr) {
    actions_ptr = default_actions;
  }
  if (actions_ptr == nullptr || actions_ptr->empty()) {
    hook_log()->debug("No hook actions configured for event '{}'", event.name);
    return;
  }
  const auto &actions = *actions_ptr;
  for (const auto &action : actions) {
    nlohmann::json action_payload = payload;
    if (!action.parameters.empty()) {
      nlohmann::json params = nlohmann::json::object();
      for (const auto &[key, value] : action.parameters) {
        params[key] = value;
      }
      action_payload["parameters"] = std::move(params);
    }
    const std::string payload_str = action_payload.dump();
    switch (action.type) {
    case HookActionType::Command:
      execute_command(action, event, payload_str);
      break;
    case HookActionType::Http:
      execute_http(action, event, payload_str);
      break;
    }
  }
}

void HookDispatcher::execute_command(const HookAction &action,
                                     const HookEvent &event,
                                     const std::string &payload) {
  if (!command_executor_) {
    command_executor_ = [](const HookAction &hook_action, const HookEvent &evt,
                           const std::string &body) {
      ScopedEnvVar event_name{"AGPM_HOOK_EVENT", evt.name};
      ScopedEnvVar payload_env{"AGPM_HOOK_PAYLOAD", body};
      ScopedEnvVar command_env{"AGPM_HOOK_COMMAND", hook_action.command};
      std::vector<std::unique_ptr<ScopedEnvVar>> parameter_envs;
      parameter_envs.reserve(hook_action.parameters.size());
      for (const auto &param : hook_action.parameters) {
        parameter_envs.push_back(std::make_unique<ScopedEnvVar>(
            parameter_env_name(param.first), param.second));
      }
      int rc = std::system(hook_action.command.c_str());
      return rc;
    };
  }
  int result = command_executor_(action, event, payload);
  if (result != 0) {
    hook_log()->warn("Hook command '{}' exited with status {}", action.command,
                     result);
  } else {
    hook_log()->debug("Hook command '{}' executed successfully",
                      action.command);
  }
}

void HookDispatcher::execute_http(const HookAction &action,
                                  const HookEvent &event,
                                  const std::string &payload) {
  if (!http_executor_) {
    http_executor_ = [](const HookAction &hook_action, const HookEvent &,
                        const std::string &body) {
      CURL *curl = curl_easy_init();
      if (!curl) {
        throw std::runtime_error("Failed to initialize curl for hook request");
      }
      curl_easy_setopt(curl, CURLOPT_URL, hook_action.endpoint.c_str());
      curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
      struct curl_slist *headers = nullptr;
      bool has_content_type = false;
      for (const auto &header : hook_action.headers) {
        std::string line = header.first + ": " + header.second;
        headers = curl_slist_append(headers, line.c_str());
        if (!has_content_type) {
          std::string lower = header.first;
          std::transform(lower.begin(), lower.end(), lower.begin(),
                         [](unsigned char c) { return static_cast<char>(
                                                   std::tolower(c)); });
          if (lower == "content-type") {
            has_content_type = true;
          }
        }
      }
      if (!has_content_type) {
        headers = curl_slist_append(headers, "Content-Type: application/json");
      }
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                       static_cast<long>(body.size()));
      std::string method = hook_action.method;
      if (!method.empty()) {
        std::transform(method.begin(), method.end(), method.begin(),
                       [](unsigned char c) {
                         return static_cast<char>(std::toupper(c));
                       });
      }
      if (method == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
      } else if (method == "POST" || method.empty()) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
      } else {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, hook_action.method.c_str());
      }
      CURLcode res = curl_easy_perform(curl);
      long status = 0;
      if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
      }
      curl_slist_free_all(headers);
      curl_easy_cleanup(curl);
      if (res != CURLE_OK) {
        throw std::runtime_error(std::string("Hook HTTP request failed: ") +
                                 curl_easy_strerror(res));
      }
      return status;
    };
  }
  long status = http_executor_(action, event, payload);
  if (status >= 200 && status < 300) {
    hook_log()->debug("Hook HTTP {} {} responded with {}", action.method,
                      action.endpoint, status);
  } else if (status != 0) {
    hook_log()->warn("Hook HTTP {} {} responded with status {}", action.method,
                     action.endpoint, status);
  }
}

std::optional<std::string>
HookDispatcher::extract_repository(const HookEvent &event) {
  auto owner_it = event.data.find("owner");
  auto repo_it = event.data.find("repo");
  if (owner_it == event.data.end() || repo_it == event.data.end()) {
    return std::nullopt;
  }
  if (!owner_it->is_string() || !repo_it->is_string()) {
    return std::nullopt;
  }
  return owner_it->get<std::string>() + "/" + repo_it->get<std::string>();
}

const RepositoryHookSettings *
HookDispatcher::match_repository_override(const HookEvent &event) const {
  auto repository = extract_repository(event);
  if (!repository) {
    return nullptr;
  }
  for (const auto &entry : repo_overrides_) {
    if (entry.compiled_pattern) {
      if (std::regex_match(*repository, *entry.compiled_pattern)) {
        return &entry;
      }
    } else if (entry.pattern == *repository) {
      return &entry;
    }
  }
  return nullptr;
}

bool HookDispatcher::has_actions() const {
  if (!settings_.default_actions.empty()) {
    return true;
  }
  for (const auto &kv : settings_.event_actions) {
    if (!kv.second.empty()) {
      return true;
    }
  }
  for (const auto &entry : repo_overrides_) {
    if (entry.has_enabled) {
      return true;
    }
    if (entry.overrides_default_actions && !entry.default_actions.empty()) {
      return true;
    }
    if (entry.overrides_event_actions) {
      for (const auto &kv : entry.event_actions) {
        if (!kv.second.empty()) {
          return true;
        }
      }
    }
  }
  return false;
}

} // namespace agpm
