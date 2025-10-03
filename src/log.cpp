#include "log.hpp"
#include <memory>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <vector>

namespace {
std::weak_ptr<spdlog::logger> g_logger;
}

namespace agpm {

void init_logger(spdlog::level::level_enum level, const std::string &pattern,
                 const std::string &file) {
  auto logger = spdlog::get("agpm");
  if (!logger) {
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    if (!file.empty()) {
      sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
          file, 1024 * 1024 * 5, 3));
    }
    logger =
        std::make_shared<spdlog::logger>("agpm", sinks.begin(), sinks.end());
    spdlog::set_default_logger(logger);
    g_logger = logger;
  }
  logger->set_level(level);
  if (!pattern.empty()) {
    spdlog::set_pattern(pattern);
  }
}

void ensure_default_logger() {
  auto logger = spdlog::default_logger();
  auto locked = g_logger.lock();
  if (!logger || !locked || logger.get() != locked.get()) {
    init_logger(spdlog::level::info);
  }
}

} // namespace agpm
