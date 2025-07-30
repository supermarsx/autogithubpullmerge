#ifndef AUTOGITHUBPULLMERGE_LOG_HPP
#define AUTOGITHUBPULLMERGE_LOG_HPP

#include <memory>
#include <spdlog/spdlog.h>

namespace agpm {

/** Initialize the global logger with the given level and optional file. */
void init_logger(spdlog::level::level_enum level = spdlog::level::info,
                 const std::string &file = "");

/** Convert a string to an spdlog level. */
spdlog::level::level_enum level_from_string(const std::string &level);

/** Retrieve the global logger instance. */
std::shared_ptr<spdlog::logger> logger();

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_LOG_HPP
