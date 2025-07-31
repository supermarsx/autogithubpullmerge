#include "config.hpp"
#include <cassert>
#include <fstream>

int main() {
  // YAML config enabling verbose
  {
    std::ofstream f("cfg.yaml");
    f << "verbose: true\npoll_interval: 3\nmax_request_rate: 10\n";
    f.close();
  }
  agpm::Config yaml_cfg = agpm::Config::from_file("cfg.yaml");
  assert(yaml_cfg.verbose());
  assert(yaml_cfg.poll_interval() == 3);
  assert(yaml_cfg.max_request_rate() == 10);

  // JSON config disabling verbose
  {
    std::ofstream f("cfg.json");
    f << "{\"verbose\": false, \"poll_interval\":2, \"max_request_rate\":5}";
    f.close();
  }
  agpm::Config json_cfg = agpm::Config::from_file("cfg.json");
  assert(!json_cfg.verbose());
  assert(json_cfg.poll_interval() == 2);
  assert(json_cfg.max_request_rate() == 5);

  return 0;
}
