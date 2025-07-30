#ifndef AUTOGITHUBPULLMERGE_CONFIG_HPP
#define AUTOGITHUBPULLMERGE_CONFIG_HPP

#include <string>

namespace agpm {

/// Application configuration loaded from a YAML or JSON file.
class Config {
public:
  /** Check whether verbose output is enabled. */
  bool verbose() const { return verbose_; }

  /// Load configuration from the file at `path`.
  static Config from_file(const std::string &path);

private:
  bool verbose_ = false;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_CONFIG_HPP
