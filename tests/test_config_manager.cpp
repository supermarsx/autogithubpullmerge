#include "config_manager.hpp"
#include <cassert>
#include <fstream>

int main() {
  agpm::ConfigManager mgr;
  {
    std::ofstream f("cfg.yaml");
    f << "verbose: true\npoll_interval: 4\nmax_request_rate: 7\n";
    f.close();
  }
  agpm::Config yaml_cfg = mgr.load("cfg.yaml");
  assert(yaml_cfg.verbose());
  assert(yaml_cfg.poll_interval() == 4);
  assert(yaml_cfg.max_request_rate() == 7);

  {
    std::ofstream f("cfg.json");
    f << "{\"verbose\": false, \"poll_interval\":1, \"max_request_rate\":3}";
    f.close();
  }
  agpm::Config json_cfg = mgr.load("cfg.json");
  assert(!json_cfg.verbose());
  assert(json_cfg.poll_interval() == 1);
  assert(json_cfg.max_request_rate() == 3);

  return 0;
}
