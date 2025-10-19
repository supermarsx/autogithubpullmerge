#include "config_manager.hpp"

namespace agpm {

/**
 * Load configuration data from disk using the Config factory helpers.
 *
 * @param path Filesystem path to the configuration file.
 * @return Parsed configuration instance.
 */
Config ConfigManager::load(const std::string &path) const {
  return Config::from_file(path);
}

} // namespace agpm
