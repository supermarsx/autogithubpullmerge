#include "config_manager.hpp"

namespace agpm {

Config ConfigManager::load(const std::string &path) const {
  return Config::from_file(path);
}

} // namespace agpm
