#include "repo_discovery.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <spdlog/spdlog.h>

namespace agpm {

namespace {

/**
 * Produce a lowercase copy of the input string.
 */
std::string normalize(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

/**
 * Trim whitespace from both ends of a string.
 */
std::string trim(const std::string &s) {
  auto first = std::find_if_not(s.begin(), s.end(),
                                [](unsigned char c) { return std::isspace(c); });
  if (first == s.end())
    return {};
  auto last = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) {
    return std::isspace(c);
  }).base();
  return last <= first ? std::string{} : std::string(first, last);
}

/**
 * Extract an owner/repo pair from a GitHub URL.
 */
std::optional<std::pair<std::string, std::string>>
parse_github_repo_from_url(const std::string &url_in) {
  std::string url = trim(url_in);
  if (url.empty())
    return std::nullopt;

  auto strip_suffix = [](std::string value) {
    constexpr std::string_view git_suffix = ".git";
    if (value.size() > git_suffix.size() &&
        value.compare(value.size() - git_suffix.size(), git_suffix.size(),
                      git_suffix.data()) == 0) {
      value.erase(value.size() - git_suffix.size());
    }
    if (!value.empty() && value.back() == '/')
      value.pop_back();
    return value;
  };

  std::string owner;
  std::string repo;
  if (url.rfind("git@github.com:", 0) == 0) {
    std::string remainder = strip_suffix(url.substr(std::string("git@github.com:").size()));
    auto slash = remainder.find('/');
    if (slash == std::string::npos)
      return std::nullopt;
    owner = remainder.substr(0, slash);
    repo = remainder.substr(slash + 1);
  } else {
    auto pos = url.find("github.com/");
    if (pos == std::string::npos) {
      pos = url.find("github.com:");
      if (pos == std::string::npos)
        return std::nullopt;
    }
    std::size_t start = pos + std::string("github.com").size();
    if (url[start] == '/' || url[start] == ':')
      ++start;
    std::string remainder = strip_suffix(url.substr(start));
    auto slash = remainder.find('/');
    if (slash == std::string::npos)
      return std::nullopt;
    owner = remainder.substr(0, slash);
    repo = remainder.substr(slash + 1);
  }
  if (owner.empty() || repo.empty())
    return std::nullopt;
  return std::make_pair(owner, repo);
}

/**
 * Parse the remote origin from a git config file.
 */
std::optional<std::pair<std::string, std::string>>
parse_origin_from_config(const std::filesystem::path &config_path) {
  std::ifstream config(config_path);
  if (!config)
    return std::nullopt;

  std::string line;
  bool in_origin = false;
  while (std::getline(config, line)) {
    std::string trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#')
      continue;
    if (trimmed.front() == '[' && trimmed.back() == ']') {
      std::string section = trimmed.substr(1, trimmed.size() - 2);
      section = trim(section);
      auto lower = normalize(section);
      if (lower.rfind("remote \"origin\"", 0) == 0) {
        in_origin = true;
      } else {
        in_origin = false;
      }
      continue;
    }
    if (!in_origin)
      continue;
    auto eq = trimmed.find('=');
    if (eq == std::string::npos)
      continue;
    std::string key = trim(trimmed.substr(0, eq));
    if (normalize(key) != "url")
      continue;
    std::string value = trim(trimmed.substr(eq + 1));
    auto parsed = parse_github_repo_from_url(value);
    if (parsed)
      return parsed;
  }
  return std::nullopt;
}

/**
 * Add a repository to the results if it has not been seen before.
 */
void try_add_repo(const std::filesystem::path &config_path,
                  const std::filesystem::path &root,
                  std::unordered_set<std::string> &seen,
                  std::vector<std::pair<std::string, std::string>> &out) {
  auto parsed = parse_origin_from_config(config_path);
  if (!parsed) {
    spdlog::warn("Skipping repository at '{}' (missing GitHub remote origin)", root.string());
    return;
  }
  std::string key = parsed->first + "/" + parsed->second;
  if (seen.insert(key).second) {
    out.push_back(*parsed);
    spdlog::info("Discovered repository {} from {}", key, root.string());
  }
}

/**
 * Examine a filesystem path to determine if it is a GitHub repository.
 */
void inspect_candidate(const std::filesystem::path &candidate,
                       std::unordered_set<std::string> &seen,
                       std::vector<std::pair<std::string, std::string>> &out) {
  std::error_code ec;
  if (!std::filesystem::exists(candidate, ec))
    return;

  // Standard repository with .git directory
  auto git_dir = candidate / ".git";
  if (std::filesystem::is_directory(git_dir, ec)) {
    auto config_path = git_dir / "config";
    if (std::filesystem::is_regular_file(config_path, ec)) {
      try_add_repo(config_path, candidate, seen, out);
    }
    return;
  }

  // Worktree reference (.git file pointing to actual dir)
  if (std::filesystem::is_regular_file(git_dir, ec)) {
    std::ifstream git_file(git_dir);
    std::string line;
    while (std::getline(git_file, line)) {
      constexpr std::string_view key = "gitdir:";
      if (line.size() > key.size() &&
          normalize(line.substr(0, key.size())) == key) {
        std::string path = trim(line.substr(key.size()));
        std::filesystem::path resolved = std::filesystem::weakly_canonical(
            candidate / path, ec);
        if (!resolved.empty()) {
          auto config_path = resolved / "config";
          if (std::filesystem::is_regular_file(config_path, ec)) {
            try_add_repo(config_path, candidate, seen, out);
          }
        }
        break;
      }
    }
    return;
  }

  // Bare repository (contains config directly)
  auto bare_config = candidate / "config";
  if (std::filesystem::is_regular_file(bare_config, ec)) {
    try_add_repo(bare_config, candidate, seen, out);
  }
}

} // namespace

/**
 * Convert a repository discovery mode to its string representation.
 */
std::string to_string(RepoDiscoveryMode mode) {
  switch (mode) {
  case RepoDiscoveryMode::Disabled:
    return "disabled";
  case RepoDiscoveryMode::All:
    return "all";
  case RepoDiscoveryMode::Filesystem:
    return "filesystem";
  }
  return "disabled";
}

/**
 * Parse a repository discovery mode from a string.
 */
RepoDiscoveryMode repo_discovery_mode_from_string(const std::string &value) {
  static const std::unordered_map<std::string, RepoDiscoveryMode> lookup = {
      {"disabled", RepoDiscoveryMode::Disabled},
      {"off", RepoDiscoveryMode::Disabled},
      {"manual", RepoDiscoveryMode::Disabled},
      {"none", RepoDiscoveryMode::Disabled},
      {"all", RepoDiscoveryMode::All},
      {"token", RepoDiscoveryMode::All},
      {"enabled", RepoDiscoveryMode::All},
      {"auto", RepoDiscoveryMode::All},
      {"filesystem", RepoDiscoveryMode::Filesystem},
      {"fs", RepoDiscoveryMode::Filesystem},
      {"local", RepoDiscoveryMode::Filesystem},
      {"directory", RepoDiscoveryMode::Filesystem}};

  auto key = normalize(value);
  auto it = lookup.find(key);
  if (it == lookup.end()) {
    throw std::invalid_argument("Unknown repo discovery mode: " + value);
  }
  return it->second;
}

/**
 * Determine whether discovery is enabled for the given mode.
 */
bool repo_discovery_enabled(RepoDiscoveryMode mode) {
  return mode != RepoDiscoveryMode::Disabled;
}

/**
 * Check if token-based discovery is required for the mode.
 */
bool repo_discovery_uses_tokens(RepoDiscoveryMode mode) {
  return mode == RepoDiscoveryMode::All;
}

/**
 * Check if filesystem scanning is required for the mode.
 */
bool repo_discovery_uses_filesystem(RepoDiscoveryMode mode) {
  return mode == RepoDiscoveryMode::Filesystem;
}

/**
 * Discover repositories by scanning filesystem roots for git metadata.
 */
std::vector<std::pair<std::string, std::string>>
discover_repositories_from_filesystem(const std::vector<std::string> &roots) {
  std::vector<std::pair<std::string, std::string>> repos;
  std::unordered_set<std::string> seen;
  std::error_code ec;

  for (const auto &root_str : roots) {
    if (root_str.empty())
      continue;
    std::filesystem::path root(root_str);
    if (!std::filesystem::exists(root, ec)) {
      spdlog::warn("Repository discovery root '{}' does not exist", root_str);
      continue;
    }
    inspect_candidate(root, seen, repos);
    if (!std::filesystem::is_directory(root, ec))
      continue;
    for (const auto &entry : std::filesystem::directory_iterator(root, ec)) {
      if (!entry.is_directory(ec))
        continue;
      inspect_candidate(entry.path(), seen, repos);
    }
  }
  return repos;
}

} // namespace agpm
