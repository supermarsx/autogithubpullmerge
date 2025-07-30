#include "app.hpp"
#include <cassert>

int main() {
  agpm::App app;
  assert(app.run(0, nullptr) == 0);
  return 0;
}
