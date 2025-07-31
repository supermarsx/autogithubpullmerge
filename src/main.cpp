#include "app.hpp"
#include "tui.hpp"

int main(int argc, char **argv) {
  agpm::App app;
  int ret = app.run(argc, argv);
  if (ret == 0) {
    agpm::Tui ui;
    ui.init();
    ui.run();
    ui.cleanup();
  }
  return ret;
}
