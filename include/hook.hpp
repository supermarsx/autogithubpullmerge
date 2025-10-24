#ifndef AUTOGITHUBPULLMERGE_HOOK_HPP
#define AUTOGITHUBPULLMERGE_HOOK_HPP

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace agpm {

/** \brief Supported hook action backends. */
enum class HookActionType {
  Command, ///< Execute a local command
  Http     ///< Dispatch an HTTP request
};

/** \brief Action executed when a hook event fires. */
struct HookAction {
  HookActionType type{HookActionType::Command};
  std::string command; ///< Command to execute when @ref type is Command
  std::string endpoint; ///< Endpoint to call when @ref type is Http
  std::string method{"POST"}; ///< HTTP method for Http actions
  std::vector<std::pair<std::string, std::string>> headers; ///< Extra HTTP headers
};

/** \brief Event payload delivered to hook actions. */
struct HookEvent {
  std::string name; ///< Canonical event identifier
  nlohmann::json data = nlohmann::json::object(); ///< Structured event data
};

/** \brief Global configuration for hook dispatch. */
struct HookSettings {
  bool enabled{false}; ///< Master enable flag
  std::vector<HookAction> default_actions; ///< Actions applied to every event
  std::unordered_map<std::string, std::vector<HookAction>>
      event_actions; ///< Optional per-event action overrides
  int pull_threshold{0};   ///< Trigger hook when total pulls exceed this value
  int branch_threshold{0}; ///< Trigger hook when branches exceed this value
};

/**
 * @brief Asynchronous dispatcher that executes hook actions on a dedicated
 * worker thread.
 */
class HookDispatcher {
public:
  using CommandExecutor =
      std::function<int(const HookAction &, const HookEvent &,
                        const std::string &)>;
  using HttpExecutor =
      std::function<long(const HookAction &, const HookEvent &,
                         const std::string &)>;

  explicit HookDispatcher(HookSettings settings,
                          CommandExecutor command_executor = CommandExecutor{},
                          HttpExecutor http_executor = HttpExecutor{});
  ~HookDispatcher();

  HookDispatcher(const HookDispatcher &) = delete;
  HookDispatcher &operator=(const HookDispatcher &) = delete;

  /**
   * Enqueue a hook event for asynchronous processing.
   */
  void enqueue(HookEvent event);

  /**
   * Access immutable dispatch settings.
   */
  const HookSettings &settings() const { return settings_; }

private:
  void worker();
  void dispatch(const HookEvent &event);
  void execute_command(const HookAction &action, const HookEvent &event,
                       const std::string &payload);
  void execute_http(const HookAction &action, const HookEvent &event,
                    const std::string &payload);
  bool has_actions() const;

  HookSettings settings_;
  CommandExecutor command_executor_;
  HttpExecutor http_executor_;
  std::thread thread_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<HookEvent> queue_;
  bool running_{false};
  bool stop_{false};
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_HOOK_HPP
