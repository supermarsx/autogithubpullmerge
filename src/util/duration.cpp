#include "util/duration.hpp"

#include <cctype>
#include <stdexcept>

namespace agpm {

/**
 * Parse a human-readable duration string (e.g., "5m", "2h").
 *
 * @param str Duration string comprised of number/unit pairs.
 * @return Parsed duration in seconds.
 * @throws std::runtime_error When the format or unit is invalid.
 */
std::chrono::seconds parse_duration(const std::string &str) {
  if (str.empty()) {
    return std::chrono::seconds{0};
  }

  using namespace std::chrono;
  long total = 0;
  std::size_t i = 0;
  bool has_unit = false;

  while (i < str.size()) {
    if (!std::isdigit(static_cast<unsigned char>(str[i]))) {
      throw std::runtime_error("Invalid duration string");
    }

    long value = 0;
    while (i < str.size() && std::isdigit(static_cast<unsigned char>(str[i]))) {
      value = value * 10 + (str[i] - '0');
      ++i;
    }

    if (i == str.size()) {
      if (has_unit) {
        throw std::runtime_error("Missing unit in duration");
      }
      total += value; // plain seconds
      break;
    }

    char unit = std::tolower(static_cast<unsigned char>(str[i]));
    ++i;
    switch (unit) {
    case 's':
      total += value;
      break;
    case 'm':
      total += value * 60;
      break;
    case 'h':
      total += value * 3600;
      break;
    case 'd':
      total += value * 86400;
      break;
    case 'w':
      total += value * 604800;
      break;
    default:
      throw std::runtime_error("Invalid duration suffix");
    }
    has_unit = true;
  }

  return seconds{total};
}

} // namespace agpm
