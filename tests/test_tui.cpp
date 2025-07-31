#include "tui.hpp"
#include <cstdlib>

int main() {
  setenv("TERM", "xterm", 1);
  agpm::Tui ui;
  ui.init();
  ui.cleanup();
  return 0;
}
