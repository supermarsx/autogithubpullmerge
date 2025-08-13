#ifndef AUTOGITHUBPULLMERGE_LOG_HPP
#define AUTOGITHUBPULLMERGE_LOG_HPP

#include <spdlog/spdlog.h>
#include <string>

namespace agpm {

/**
 * Initialize global logger with console and optional rotating file sink.
 *
 * @param level   Logging verbosity level.
 * @param pattern Log message pattern. Empty string keeps spdlog default.
 * @param file    Optional log file path for rotating sink.
 */
void init_logger(spdlog::level::level_enum level,
                 const std::string &pattern = "", const std::string &file = "");

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_LOG_HPP
