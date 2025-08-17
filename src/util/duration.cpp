#include "util/duration.hpp"

#include <cctype>
#include <stdexcept>

namespace agpm {

std::chrono::seconds parse_duration(const std::string &str) {
  if (str.empty())
    return std::chrono::seconds{0};
  char unit = str.back();
  std::string num = str;
  if (!std::isdigit(static_cast<unsigned char>(unit))) {
    num.pop_back();
  } else {
    unit = 's';
  }
  long value = std::stol(num);
  switch (std::tolower(static_cast<unsigned char>(unit))) {
  case 's':
    return std::chrono::seconds{value};
  case 'm':
    return std::chrono::seconds{value * 60};
  case 'h':
    return std::chrono::seconds{value * 3600};
  case 'd':
    return std::chrono::seconds{value * 86400};
  case 'w':
    return std::chrono::seconds{value * 604800};
  default:
    throw std::runtime_error("Invalid duration suffix");
  }
}

} // namespace agpm
