#include "tui.hpp"
#include <cassert>
#include <cstdlib>
#include <vector>

int main() {
  setenv("TERM", "xterm", 1);
  agpm::Tui ui;
  std::vector<agpm::PullRequest> prs; // empty to exit immediately
  assert(ui.run(prs) == 0);
  return 0;
}
