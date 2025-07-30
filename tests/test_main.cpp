#include "app.hpp"
#include <cassert>
#include <vector>

int main() {
  agpm::App app;
  std::vector<char *> args;
  char prog[] = "tests";
  char verbose[] = "--verbose";
  args.push_back(prog);
  args.push_back(verbose);
  assert(app.run(static_cast<int>(args.size()), args.data()) == 0);
  assert(app.options().verbose);
  return 0;
}
