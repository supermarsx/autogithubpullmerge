/**
 * @file rule_engine.cpp
 * @brief Implements rule engines for pull request and branch actions.
 *
 * This file defines the PullRequestRuleEngine and BranchRuleEngine classes,
 * which map repository state to merge, wait, ignore, or delete actions.
 */
#include "rule_engine.hpp"

#include <algorithm>
#include <cctype>

namespace agpm {
namespace {
std::string normalize_state(const std::string &state) {
  std::string copy = state;
  std::transform(copy.begin(), copy.end(), copy.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return copy;
}
} // namespace

PullRequestRuleEngine::PullRequestRuleEngine() {
  state_actions_["dirty"] = PullRequestAction::kClose;
  state_actions_["clean"] = PullRequestAction::kMerge;
  state_actions_["blocked"] = PullRequestAction::kMerge;
  state_actions_["unstable"] = PullRequestAction::kMerge;
  state_actions_["failure"] = PullRequestAction::kMerge;
  state_actions_["failed"] = PullRequestAction::kMerge;
  state_actions_["rejected"] = PullRequestAction::kMerge;
}

void PullRequestRuleEngine::set_action(const std::string &state,
                                       PullRequestAction action) {
  state_actions_[normalize_state(state)] = action;
}

PullRequestAction
PullRequestRuleEngine::action_for_state(const std::string &state) const {
  auto key = normalize_state(state);
  auto it = state_actions_.find(key);
  if (it == state_actions_.end()) {
    return PullRequestAction::kNone;
  }
  return it->second;
}

PullRequestAction
PullRequestRuleEngine::decide(const PullRequestMetadata &metadata) const {
  if (!metadata.state.empty() && normalize_state(metadata.state) != "open") {
    return PullRequestAction::kIgnore;
  }

  if (metadata.draft) {
    return PullRequestAction::kWait;
  }

  PullRequestAction action = action_for_state(metadata.mergeable_state);
  if (action != PullRequestAction::kNone) {
    return action;
  }

  if (metadata.check_state == PullRequestCheckState::Passed ||
      metadata.check_state == PullRequestCheckState::Rejected) {
    return PullRequestAction::kMerge;
  }

  return PullRequestAction::kWait;
}

BranchRuleEngine::BranchRuleEngine() {
  state_actions_["stray"] = BranchAction::kDelete;
  state_actions_["new"] = BranchAction::kKeep;
  state_actions_["active"] = BranchAction::kKeep;
  state_actions_["dirty"] = BranchAction::kDelete;
  state_actions_["purge"] = BranchAction::kDelete;
}

void BranchRuleEngine::set_action(const std::string &state,
                                  BranchAction action) {
  state_actions_[normalize_state(state)] = action;
}

BranchAction
BranchRuleEngine::action_for_state(const std::string &state) const {
  auto key = normalize_state(state);
  auto it = state_actions_.find(key);
  if (it == state_actions_.end()) {
    return BranchAction::kNone;
  }
  return it->second;
}

BranchAction BranchRuleEngine::decide(const BranchMetadata &metadata) const {
  if (!metadata.state.empty()) {
    BranchAction configured = action_for_state(metadata.state);
    if (configured != BranchAction::kNone) {
      return configured;
    }
  }
  if (metadata.stray) {
    BranchAction stray_action = action_for_state("stray");
    if (stray_action != BranchAction::kNone) {
      return stray_action;
    }
  }
  if (metadata.newly_created) {
    BranchAction new_action = action_for_state("new");
    if (new_action != BranchAction::kNone) {
      return new_action;
    }
  }
  return BranchAction::kKeep;
}

} // namespace agpm
