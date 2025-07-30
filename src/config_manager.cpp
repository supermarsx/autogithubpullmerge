#include "config_manager.hpp"
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

namespace agpm {

Config ConfigManager::load(const std::string &path) const {
  auto pos = path.find_last_of('.');
  if (pos == std::string::npos) {
    throw std::runtime_error("Unknown config file extension");
  }
  std::string ext = path.substr(pos + 1);
  Config cfg;
  if (ext == "yaml" || ext == "yml") {
    YAML::Node node = YAML::LoadFile(path);
    if (node["verbose"]) {
      cfg.set_verbose(node["verbose"].as<bool>());
    }
  } else if (ext == "json") {
    std::ifstream f(path);
    if (!f) {
      throw std::runtime_error("Failed to open config file");
    }
    nlohmann::json j;
    f >> j;
    if (j.contains("verbose")) {
      cfg.set_verbose(j["verbose"].get<bool>());
    }
  } else {
    throw std::runtime_error("Unsupported config format");
  }
  return cfg;
}

} // namespace agpm
