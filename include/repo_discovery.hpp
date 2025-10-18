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
  Filesystem  ///< Discover repositories by scanning local git directories
};

/// Convert repo discovery mode to lowercase string.
std::string to_string(RepoDiscoveryMode mode);

/// Parse a string into a repo discovery mode.
RepoDiscoveryMode repo_discovery_mode_from_string(const std::string &value);

/// Whether discovery should enumerate repositories automatically.
bool repo_discovery_enabled(RepoDiscoveryMode mode);

/// Whether the discovery mode requires GitHub token listing.
bool repo_discovery_uses_tokens(RepoDiscoveryMode mode);

/// Whether the discovery mode enumerates local filesystem repositories.
bool repo_discovery_uses_filesystem(RepoDiscoveryMode mode);

/// Discover repositories by scanning git directories under the provided roots.
std::vector<std::pair<std::string, std::string>>
discover_repositories_from_filesystem(const std::vector<std::string> &roots);

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_REPO_DISCOVERY_HPP
