#include "config_manager.hpp"
#include <cassert>
#include <fstream>

int main() {
  agpm::ConfigManager mgr;
  {
    std::ofstream f("cfg.yaml");
    f << "verbose: true\n";
    f << "poll_interval: 4\n";
    f << "max_request_rate: 7\n";
    f << "log_level: info\n";
    f.close();
  }
  agpm::Config yaml_cfg = mgr.load("cfg.yaml");
  assert(yaml_cfg.verbose());
  assert(yaml_cfg.poll_interval() == 4);
  assert(yaml_cfg.max_request_rate() == 7);
  assert(yaml_cfg.log_level() == "info");

  {
    std::ofstream f("cfg.json");
    f << "{";
    f << "\"verbose\": false,";
    f << "\"poll_interval\":1,";
    f << "\"max_request_rate\":3,";
    f << "\"log_level\":\"error\"";
    f << "}";
    f.close();
  }
  agpm::Config json_cfg = mgr.load("cfg.json");
  assert(!json_cfg.verbose());
  assert(json_cfg.poll_interval() == 1);
  assert(json_cfg.max_request_rate() == 3);
  assert(json_cfg.log_level() == "error");

  return 0;
}
