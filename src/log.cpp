#include "log.hpp"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace agpm {

static std::shared_ptr<spdlog::logger> global_logger;

spdlog::level::level_enum level_from_string(const std::string &level) {
  if (level == "trace")
    return spdlog::level::trace;
  if (level == "debug")
    return spdlog::level::debug;
  if (level == "info")
    return spdlog::level::info;
  if (level == "warn")
    return spdlog::level::warn;
  if (level == "error")
    return spdlog::level::err;
  if (level == "critical")
    return spdlog::level::critical;
  if (level == "off")
    return spdlog::level::off;
  return spdlog::level::info;
}

void init_logger(spdlog::level::level_enum level, const std::string &file) {
  std::vector<spdlog::sink_ptr> sinks;
  sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
  if (!file.empty()) {
    sinks.push_back(
        std::make_shared<spdlog::sinks::basic_file_sink_mt>(file, true));
  }
  global_logger =
      std::make_shared<spdlog::logger>("agpm", begin(sinks), end(sinks));
  spdlog::register_logger(global_logger);
  global_logger->set_level(level);
  global_logger->flush_on(level);
}

std::shared_ptr<spdlog::logger> logger() {
  if (!global_logger) {
    global_logger = spdlog::default_logger();
  }
  return global_logger;
}

} // namespace agpm
