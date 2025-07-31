#include "log.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>

namespace agpm {

void init_logger(spdlog::level::level_enum level) {
  auto logger = spdlog::get("agpm");
  if (!logger) {
    logger = spdlog::stdout_color_mt("agpm");
    spdlog::set_default_logger(logger);
  }
  logger->set_level(level);
}

} // namespace agpm
