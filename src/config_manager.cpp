/**
 * @file config_manager.cpp
 * @brief Configuration file loader implementation for autogithubpullmerge.
 *
 * Implements the ConfigManager utility for loading configuration files from
 * disk.
 */

#include "config_manager.hpp"

namespace agpm {

/**
 * Load configuration data from disk using the Config factory helpers.
 *
 * @param path Filesystem path to the configuration file.
 * @return Parsed configuration instance.
 * @throws std::runtime_error When the file cannot be read or parsed.
 */
Config ConfigManager::load(const std::string &path) const {
  return Config::from_file(path);
}

} // namespace agpm
