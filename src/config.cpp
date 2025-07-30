#include "config.hpp"
#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

namespace agpm {

Config Config::from_file(const std::string &path) {
  Config cfg;
  auto pos = path.find_last_of('.');
  if (pos == std::string::npos) {
    throw std::runtime_error("Unknown config file extension");
  }
  std::string ext = path.substr(pos + 1);
  if (ext == "yaml" || ext == "yml") {
    YAML::Node node = YAML::LoadFile(path);
    if (node["verbose"]) {
      cfg.verbose_ = node["verbose"].as<bool>();
    }
    if (node["log_level"]) {
      cfg.log_level_ = node["log_level"].as<std::string>();
    }
    if (node["log_file"]) {
      cfg.log_file_ = node["log_file"].as<std::string>();
    }
  } else if (ext == "json") {
    std::ifstream f(path);
    if (!f) {
      throw std::runtime_error("Failed to open config file");
    }
    nlohmann::json j;
    f >> j;
    if (j.contains("verbose")) {
      cfg.verbose_ = j["verbose"].get<bool>();
    }
    if (j.contains("log_level")) {
      cfg.log_level_ = j["log_level"].get<std::string>();
    }
    if (j.contains("log_file")) {
      cfg.log_file_ = j["log_file"].get<std::string>();
    }
  } else {
    throw std::runtime_error("Unsupported config format");
  }
  return cfg;
}

} // namespace agpm
