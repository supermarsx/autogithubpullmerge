#include "token_loader.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <toml++/toml.h>
#include <yaml-cpp/yaml.h>

namespace agpm {

/**
 * Load personal access tokens from a supported configuration file.
 *
 * @param path Filesystem path to the token file.
 * @return Ordered list of tokens discovered in the file.
 * @throws std::runtime_error When the file cannot be parsed or the extension
 *         is unknown.
 */
std::vector<std::string> load_tokens_from_file(const std::string &path) {
  auto pos = path.find_last_of('.');
  if (pos == std::string::npos) {
    throw std::runtime_error("Unknown token file extension");
  }
  std::string ext = path.substr(pos + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  std::vector<std::string> tokens;
  if (ext == "yaml" || ext == "yml") {
    YAML::Node node = YAML::LoadFile(path);
    if (node.IsSequence()) {
      tokens.reserve(tokens.size() + node.size());
      std::transform(node.begin(), node.end(), std::back_inserter(tokens),
                     [](const YAML::Node &n) { return n.as<std::string>(); });
    } else if (node.IsScalar()) {
      tokens.push_back(node.as<std::string>());
    }
    if (node["token"]) {
      tokens.push_back(node["token"].as<std::string>());
    }
    if (node["tokens"]) {
      const YAML::Node tokens_node = node["tokens"];
      if (!tokens_node.IsSequence()) {
        throw std::runtime_error("YAML tokens entry must be a sequence");
      }
      tokens.reserve(tokens.size() + tokens_node.size());
      std::transform(tokens_node.begin(), tokens_node.end(),
                     std::back_inserter(tokens),
                     [](const YAML::Node &n) { return n.as<std::string>(); });
    }
  } else if (ext == "json") {
    std::ifstream f(path);
    if (!f) {
      throw std::runtime_error("Failed to open token file");
    }
    nlohmann::json j;
    f >> j;
    if (j.is_array()) {
      tokens.reserve(tokens.size() + j.size());
      std::transform(j.begin(), j.end(), std::back_inserter(tokens),
                     [](const nlohmann::json &item) {
                       return item.get<std::string>();
                     });
    } else if (j.is_object()) {
      if (j.contains("token")) {
        tokens.push_back(j["token"].get<std::string>());
      }
      if (j.contains("tokens")) {
        const auto &array = j["tokens"];
        if (!array.is_array()) {
          throw std::runtime_error("JSON tokens entry must be an array");
        }
        tokens.reserve(tokens.size() + array.size());
        std::transform(array.begin(), array.end(), std::back_inserter(tokens),
                       [](const nlohmann::json &item) {
                         return item.get<std::string>();
                       });
      }
    } else if (j.is_string()) {
      tokens.push_back(j.get<std::string>());
    }
  } else if (ext == "toml" || ext == "tml") {
    toml::table tbl = toml::parse_file(path);
    if (auto single = tbl["token"].value<std::string>()) {
      tokens.push_back(*single);
    }
    if (auto arr = tbl["tokens"].as_array()) {
      tokens.reserve(tokens.size() + arr->size());
      for (const auto &item : *arr) {
        if (auto value = item.value<std::string>()) {
          tokens.push_back(*value);
        } else {
          throw std::runtime_error("TOML tokens array must contain strings");
        }
      }
    }
  } else {
    throw std::runtime_error("Unsupported token file format");
  }
  return tokens;
}

} // namespace agpm
