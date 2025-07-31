#ifndef AUTOGITHUBPULLMERGE_LOG_HPP
#define AUTOGITHUBPULLMERGE_LOG_HPP

#include <spdlog/spdlog.h>

namespace agpm {

/** Initialize the global logger with the given level. */
void init_logging(spdlog::level::level_enum level);

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_LOG_HPP
