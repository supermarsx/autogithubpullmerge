/**
 * @file log.hpp
 * @brief Logging utilities for autogithubpullmerge.
 *
 * Declares logger initialization, category loggers, and log category configuration.
 */

#ifndef AUTOGITHUBPULLMERGE_LOG_HPP
#define AUTOGITHUBPULLMERGE_LOG_HPP

#include <cstddef>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>

namespace agpm {

/**
 * Initialize the global logger with console and optional rotating file sinks.
 *
 * @param level Logging verbosity level to use for all loggers.
 * @param pattern Log message pattern. Provide an empty string to keep the
 *        underlying spdlog default.
 * @param file Optional log file path for enabling a rotating sink. When empty
 *        no file output is configured.
 * @param rotate_files Maximum number of rotated files to retain when
 *        @p file is provided.
 * @param compress_rotations Whether rotated log files should be gzip
 *        compressed automatically.
 */
void init_logger(spdlog::level::level_enum level,
                 const std::string &pattern = "", const std::string &file = "",
                 std::size_t rotate_files = 3, bool compress_rotations = false);

/**
 * Retrieve or create a logger dedicated to a specific category.
 *
 * Category loggers share sinks with the default logger so messages appear in
 * the same destinations. They allow fine-grained log-level overrides.
 *
 * @param category Arbitrary category name used as the logger identifier.
 * @return Shared pointer to the category logger.
 */
std::shared_ptr<spdlog::logger> category_logger(const std::string &category);

/**
 * Apply log level overrides for specific categories.
 *
 * @param overrides Mapping of category name to desired log level.
 */
void configure_log_categories(
    const std::unordered_map<std::string, spdlog::level::level_enum>
        &overrides);

/**
 * Ensure a default logger exists before logging.
 *
 * Calling spdlog logging macros requires a default logger. This helper creates
 * one on demand when the logging subsystem has not been explicitly
 * initialized.
 */
void ensure_default_logger();

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_LOG_HPP
