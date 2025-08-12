#include "tui.hpp"
#include <cstdlib>

int main() {
  // Ensure TERM is set so curses initializes correctly on all platforms.
#ifdef _WIN32
  _putenv_s("TERM", "xterm");
#else
  setenv("TERM", "xterm", 1);
#endif
  agpm::Tui ui;
  ui.init();
  ui.cleanup();
  return 0;
}
