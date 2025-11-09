/**
 * @file repo_discovery.hpp
 * @brief Repository discovery modes and utilities.
 *
 * Provides enums and functions for discovering GitHub repositories via tokens
 * or filesystem scanning, and for converting between string representations and
 * discovery modes.
 */
#ifndef AUTOGITHUBPULLMERGE_REPO_DISCOVERY_HPP
#define AUTOGITHUBPULLMERGE_REPO_DISCOVERY_HPP

#include <string>
#include <utility>
#include <vector>

namespace agpm {

/// Modes supported for repository discovery.
enum class RepoDiscoveryMode {
  Disabled,   ///< Use only explicitly specified repositories
  All,        ///< Discover all repositories accessible to the token(s)
  Filesystem, ///< Discover repositories by scanning local git directories
  Both        ///< Combine token-based and filesystem discovery
};

/**
 * @brief Convert repo discovery mode to lowercase string.
 * @param mode The repository discovery mode.
 * @return Lowercase string representation of the mode.
 */
std::string to_string(RepoDiscoveryMode mode);

/**
 * @brief Parse a string into a repo discovery mode.
 * @param value String to parse (case-insensitive).
 * @return Parsed RepoDiscoveryMode value.
 */
RepoDiscoveryMode repo_discovery_mode_from_string(const std::string &value);

/**
 * @brief Whether discovery should enumerate repositories automatically.
 * @param mode The repository discovery mode.
 * @return True if discovery is enabled, false otherwise.
 */
bool repo_discovery_enabled(RepoDiscoveryMode mode);

/**
 * @brief Whether the discovery mode requires GitHub token listing.
 * @param mode The repository discovery mode.
 * @return True if token-based discovery is used.
 */
bool repo_discovery_uses_tokens(RepoDiscoveryMode mode);

/**
 * @brief Whether the discovery mode enumerates local filesystem repositories.
 * @param mode The repository discovery mode.
 * @return True if filesystem-based discovery is used.
 */
bool repo_discovery_uses_filesystem(RepoDiscoveryMode mode);

/**
 * @brief Discover repositories by scanning git directories under the provided
 * roots.
 * @param roots List of root directories to scan.
 * @return Vector of (owner, repo) pairs discovered.
 */
std::vector<std::pair<std::string, std::string>>
discover_repositories_from_filesystem(const std::vector<std::string> &roots);

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_REPO_DISCOVERY_HPP
