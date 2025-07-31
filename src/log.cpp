#include "log.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>

namespace agpm {

void init_logging(spdlog::level::level_enum level) {
  auto console = spdlog::stdout_color_mt("console");
  console->set_level(level);
  spdlog::set_default_logger(console);
  spdlog::set_level(level);
}

} // namespace agpm
