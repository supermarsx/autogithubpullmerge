#include "sort.hpp"
#include <cassert>
#include <vector>

using namespace agpm;

int main() {
  std::vector<PullRequest> base = {{1, "PR2"}, {2, "PR10"}, {3, "PR1"}};

  auto alpha = base;
  sort_pull_requests(alpha, "alpha");
  assert(alpha[0].title == "PR1");
  assert(alpha[1].title == "PR10");
  assert(alpha[2].title == "PR2");

  auto rev = base;
  sort_pull_requests(rev, "reverse");
  assert(rev[0].title == "PR2");
  assert(rev[1].title == "PR10");
  assert(rev[2].title == "PR1");

  auto anum = base;
  sort_pull_requests(anum, "alphanum");
  assert(anum[0].title == "PR1");
  assert(anum[1].title == "PR2");
  assert(anum[2].title == "PR10");

  auto ranum = base;
  sort_pull_requests(ranum, "reverse-alphanum");
  assert(ranum[0].title == "PR10");
  assert(ranum[1].title == "PR2");
  assert(ranum[2].title == "PR1");

  return 0;
}
