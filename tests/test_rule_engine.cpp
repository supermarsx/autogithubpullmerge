#include "rule_engine.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace agpm;

TEST_CASE("dirty pull requests close by default") {
  PullRequestRuleEngine engine;
  PullRequestMetadata meta{};
  meta.state = "open";
  meta.mergeable_state = "dirty";
  REQUIRE(engine.decide(meta) == PullRequestAction::kClose);
}

TEST_CASE("clean pull requests merge when checks pass") {
  PullRequestRuleEngine engine;
  PullRequestMetadata meta{};
  meta.state = "open";
  meta.mergeable_state = "clean";
  meta.check_state = PullRequestCheckState::Passed;
  REQUIRE(engine.decide(meta) == PullRequestAction::kMerge);
}

TEST_CASE("rejected tests still merge by default") {
  PullRequestRuleEngine engine;
  PullRequestMetadata meta{};
  meta.state = "open";
  meta.mergeable_state = "blocked";
  meta.check_state = PullRequestCheckState::Rejected;
  REQUIRE(engine.decide(meta) == PullRequestAction::kMerge);
}

TEST_CASE("stray branches delete by default") {
  BranchRuleEngine engine;
  BranchMetadata meta{"me", "repo", "feature", "stray", true};
  REQUIRE(engine.decide(meta) == BranchAction::kDelete);
}

TEST_CASE("new branches are retained") {
  BranchRuleEngine engine;
  BranchMetadata meta{"me", "repo", "feature", "new", false, true};
  REQUIRE(engine.decide(meta) == BranchAction::kKeep);
}
