#ifndef AUTOGITHUBPULLMERGE_RULE_ENGINE_HPP
#define AUTOGITHUBPULLMERGE_RULE_ENGINE_HPP

#include "github_client.hpp"

#include <string>
#include <unordered_map>

namespace agpm {

/**
 * @brief Supported automated actions for pull requests.
 */
enum class PullRequestAction {
  kNone,   ///< No action configured for this state.
  kWait,   ///< Defer handling and revisit later.
  kIgnore, ///< Ignore the pull request entirely.
  kMerge,  ///< Merge the pull request.
  kClose   ///< Close the pull request without merging.
};

/**
 * @brief Supported automated actions for branches.
 */
enum class BranchAction {
  kNone,   ///< No action configured for this state.
  kKeep,   ///< Retain the branch.
  kIgnore, ///< Ignore the branch for reporting.
  kDelete  ///< Delete the branch.
};

/**
 * @brief Metadata describing a branch under evaluation.
 *
 * Instances are passed to the branch rule engine so that rule decisions can
 * consider both explicit state labels and contextual signals such as whether a
 * branch was newly observed or classified as stray.
 */
struct BranchMetadata {
  std::string owner;           ///< Repository owner of the branch.
  std::string repo;            ///< Repository name containing the branch.
  std::string name;            ///< Fully-qualified branch ref name.
  std::string state;           ///< Branch state label ("stray", "new", "dirty", ...).
  bool stray{false};           ///< Flag indicating the branch was classified as stray.
  bool newly_created{false};   ///< Branch observed for the first time this poll.
};

/**
 * @brief Rule based evaluator that determines automated actions for pull
 * requests.
 *
 * The rule engine maps mergeability states reported by GitHub to high-level
 * actions such as merging, waiting, or closing the pull request. Defaults follow
 * the product specification (dirty PRs close, clean/blocked/unstable PRs merge),
 * while callers may override individual states at runtime.
 */
class PullRequestRuleEngine {
public:
  /**
   * @brief Construct a rule engine with default pull request mappings.
   */
  PullRequestRuleEngine();

  /**
   * @brief Determine the action for the provided pull request metadata.
   *
   * @param metadata Metadata describing a pull request's mergeability state,
   *        draft status, and CI results.
   * @return The automation action that should be taken for the pull request.
   */
  PullRequestAction decide(const PullRequestMetadata &metadata) const;

  /**
   * @brief Override the action associated with a particular mergeable state.
   *
   * @param state Mergeability state reported by GitHub (e.g. "dirty").
   * @param action Automation action to associate with the state.
   */
  void set_action(const std::string &state, PullRequestAction action);

  /**
   * @brief Lookup the configured action for a mergeable state string.
   *
   * @param state Mergeability state label.
   * @return The action configured for @p state, or PullRequestAction::kNone.
   */
  PullRequestAction action_for_state(const std::string &state) const;

private:
  std::unordered_map<std::string, PullRequestAction> state_actions_;
};

/**
 * @brief Rule based evaluator for branch management actions.
 *
 * The engine applies defaults for stray, dirty, and purge states while still
 * allowing per-state overrides. Rule evaluation falls back to contextual flags
 * when a state label does not have an explicit mapping.
 */
class BranchRuleEngine {
public:
  /**
   * @brief Construct a branch rule engine with default state mappings.
   */
  BranchRuleEngine();

  /**
   * @brief Determine the configured action for the supplied branch metadata.
   *
   * @param metadata Metadata describing the branch under evaluation.
   * @return The automation action that should be taken for the branch.
   */
  BranchAction decide(const BranchMetadata &metadata) const;

  /**
   * @brief Override the action associated with a particular branch state.
   *
   * @param state Branch state label (e.g. "stray", "new").
   * @param action Automation action to associate with the state.
   */
  void set_action(const std::string &state, BranchAction action);

  /**
   * @brief Lookup the configured action for a branch state.
   *
   * @param state Branch state label to query.
   * @return The action configured for @p state, or BranchAction::kNone.
   */
  BranchAction action_for_state(const std::string &state) const;

private:
  std::unordered_map<std::string, BranchAction> state_actions_;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_RULE_ENGINE_HPP
