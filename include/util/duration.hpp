/**
 * @file duration.hpp
 * @brief Human-readable duration parsing utilities.
 *
 * Provides functions to parse duration strings (e.g. "10s", "5m", "2h") into
 * std::chrono::seconds for use in configuration and scheduling.
 */
#ifndef AUTOGITHUBPULLMERGE_UTIL_DURATION_HPP
#define AUTOGITHUBPULLMERGE_UTIL_DURATION_HPP

#include <chrono>
#include <string>

namespace agpm {

/**
 * Parse a human-readable duration string (e.g. "10s", "5m", "2h", "3d",
 * "1w", "1h30m") into a std::chrono::seconds value. Multiple units can be
 * combined and a pure number is interpreted as seconds.
 *
 * @param str Duration string; empty string returns zero seconds.
 * @return Parsed duration in seconds.
 * @throws std::runtime_error if an invalid format or suffix is provided.
 */
std::chrono::seconds parse_duration(const std::string &str);

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_UTIL_DURATION_HPP
