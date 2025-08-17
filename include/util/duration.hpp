#ifndef AUTOGITHUBPULLMERGE_UTIL_DURATION_HPP
#define AUTOGITHUBPULLMERGE_UTIL_DURATION_HPP

#include <chrono>
#include <string>

namespace agpm {

/**
 * Parse a human-readable duration string (e.g. "10s", "5m", "2h", "3d", "1w")
 * into a std::chrono::seconds value.
 *
 * @param str Duration string; empty string returns zero seconds.
 * @return Parsed duration in seconds.
 * @throws std::runtime_error if an invalid suffix is provided.
 */
std::chrono::seconds parse_duration(const std::string &str);

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_UTIL_DURATION_HPP
