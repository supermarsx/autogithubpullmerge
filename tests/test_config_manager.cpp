#include "config_manager.hpp"
#include <cassert>
#include <fstream>

int main() {
  agpm::ConfigManager mgr;
  {
    std::ofstream f("cfg.yaml");
    f << "verbose: true\n";
    f.close();
  }
  agpm::Config yaml_cfg = mgr.load("cfg.yaml");
  assert(yaml_cfg.verbose());

  {
    std::ofstream f("cfg.json");
    f << "{\"verbose\": false}";
    f.close();
  }
  agpm::Config json_cfg = mgr.load("cfg.json");
  assert(!json_cfg.verbose());

  return 0;
}
