#include "sort.hpp"
#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace agpm;

TEST_CASE("test sort") {
  std::vector<PullRequest> base = {{1, "PR2"}, {2, "PR10"}, {3, "PR1"}};

  auto alpha = base;
  sort_pull_requests(alpha, "alpha");
  REQUIRE(alpha[0].title == "PR1");
  REQUIRE(alpha[1].title == "PR10");
  REQUIRE(alpha[2].title == "PR2");

  auto rev = base;
  sort_pull_requests(rev, "reverse");
  REQUIRE(rev[0].title == "PR2");
  REQUIRE(rev[1].title == "PR10");
  REQUIRE(rev[2].title == "PR1");

  auto anum = base;
  sort_pull_requests(anum, "alphanum");
  REQUIRE(anum[0].title == "PR1");
  REQUIRE(anum[1].title == "PR2");
  REQUIRE(anum[2].title == "PR10");

  auto ranum = base;
  sort_pull_requests(ranum, "reverse-alphanum");
  REQUIRE(ranum[0].title == "PR10");
  REQUIRE(ranum[1].title == "PR2");
  REQUIRE(ranum[2].title == "PR1");
}
