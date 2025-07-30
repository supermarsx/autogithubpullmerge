#include "config.hpp"
#include <cassert>
#include <fstream>

int main() {
  // YAML config enabling verbose
  {
    std::ofstream f("cfg.yaml");
    f << "verbose: true\n";
    f.close();
  }
  agpm::Config yaml_cfg = agpm::Config::from_file("cfg.yaml");
  assert(yaml_cfg.verbose());

  // JSON config disabling verbose
  {
    std::ofstream f("cfg.json");
    f << "{\"verbose\": false}";
    f.close();
  }
  agpm::Config json_cfg = agpm::Config::from_file("cfg.json");
  assert(!json_cfg.verbose());

  return 0;
}
