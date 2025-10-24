#ifndef AUTOGITHUBPULLMERGE_STRAY_DETECTION_MODE_HPP
#define AUTOGITHUBPULLMERGE_STRAY_DETECTION_MODE_HPP

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>

namespace agpm {

/** \brief Selection for stray branch detection engines. */
enum class StrayDetectionMode {
  RuleBased,   ///< Use the deterministic rule-based engine only.
  Heuristic,   ///< Use the heuristic engine only.
  Combined     ///< Run both engines and merge their results.
};

inline bool uses_rule_based(StrayDetectionMode mode) {
  return mode == StrayDetectionMode::RuleBased ||
         mode == StrayDetectionMode::Combined;
}

inline bool uses_heuristic(StrayDetectionMode mode) {
  return mode == StrayDetectionMode::Heuristic ||
         mode == StrayDetectionMode::Combined;
}

inline std::string to_string(StrayDetectionMode mode) {
  switch (mode) {
  case StrayDetectionMode::RuleBased:
    return "rule";
  case StrayDetectionMode::Heuristic:
    return "heuristic";
  case StrayDetectionMode::Combined:
    return "both";
  }
  return "rule";
}

inline std::optional<StrayDetectionMode>
stray_detection_mode_from_string(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (value == "rule" || value == "rules" || value == "rule-based" ||
      value == "rulebased") {
    return StrayDetectionMode::RuleBased;
  }
  if (value == "heuristic" || value == "heuristics" ||
      value == "heuristic-only" || value == "heuristics-only") {
    return StrayDetectionMode::Heuristic;
  }
  if (value == "both" || value == "all" || value == "combined" ||
      value == "rule+heuristic" || value == "heuristic+rule") {
    return StrayDetectionMode::Combined;
  }
  return std::nullopt;
}

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_STRAY_DETECTION_MODE_HPP
