#ifndef AUTOGITHUBPULLMERGE_CONFIG_HPP
#define AUTOGITHUBPULLMERGE_CONFIG_HPP

#include <string>

namespace agpm {

/// Application configuration loaded from a YAML or JSON file.
class Config {
public:
  bool verbose() const { return verbose_; }
  const std::string &log_level() const { return log_level_; }
  const std::string &log_file() const { return log_file_; }
  /// Load configuration from the file at `path`.
  static Config from_file(const std::string &path);

private:
  bool verbose_ = false;
  std::string log_level_ = "info";
  std::string log_file_;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_CONFIG_HPP
