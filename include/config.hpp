#ifndef AUTOGITHUBPULLMERGE_CONFIG_HPP
#define AUTOGITHUBPULLMERGE_CONFIG_HPP

#include <string>

namespace agpm {

/// Application configuration loaded from a YAML or JSON file.
class Config {
public:
  /** Check whether verbose output is enabled. */
  bool verbose() const { return verbose_; }

  /// Set verbose output mode.
  void set_verbose(bool verbose) { verbose_ = verbose; }

  /// Polling interval in seconds.
  int poll_interval() const { return poll_interval_; }

  /// Set polling interval.
  void set_poll_interval(int interval) { poll_interval_ = interval; }

  /// Maximum requests per minute.
  int max_request_rate() const { return max_request_rate_; }

  /// Set maximum request rate.
  void set_max_request_rate(int rate) { max_request_rate_ = rate; }

  /// Load configuration from the file at `path`.
  static Config from_file(const std::string &path);

private:
  bool verbose_ = false;
  int poll_interval_ = 0;
  int max_request_rate_ = 60;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_CONFIG_HPP
