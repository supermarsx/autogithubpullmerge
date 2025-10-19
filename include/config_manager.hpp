#ifndef AUTOGITHUBPULLMERGE_CONFIG_MANAGER_HPP
#define AUTOGITHUBPULLMERGE_CONFIG_MANAGER_HPP

#include "config.hpp"
#include <string>

namespace agpm {

/// Utility class for loading configuration files.
class ConfigManager {
public:
  /**
   * Load a configuration from a YAML, TOML, or JSON file.
   *
   * @param path Path to the configuration file on disk.
   * @return Parsed configuration object.
   * @throws std::runtime_error When the file cannot be read or parsed.
   */
  Config load(const std::string &path) const;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_CONFIG_MANAGER_HPP
