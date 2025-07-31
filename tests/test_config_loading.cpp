#include "config.hpp"
#include <cassert>
#include <fstream>

int main() {
  {
    std::ofstream f("config.yaml");
    f << "verbose: true\npoll_interval: 10\nmax_request_rate: 20\n";
    f.close();
  }
  agpm::Config ycfg = agpm::Config::from_file("config.yaml");
  assert(ycfg.verbose());
  assert(ycfg.poll_interval() == 10);
  assert(ycfg.max_request_rate() == 20);

  {
    std::ofstream f("config.json");
    f << "{\"verbose\":false,\"poll_interval\":5,\"max_request_rate\":15}";
    f.close();
  }
  agpm::Config jcfg = agpm::Config::from_file("config.json");
  assert(!jcfg.verbose());
  assert(jcfg.poll_interval() == 5);
  assert(jcfg.max_request_rate() == 15);

  return 0;
}
