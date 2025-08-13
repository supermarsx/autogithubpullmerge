#include "config.hpp"
#include <cassert>
#include <fstream>

int main() {
  {
    std::ofstream f("config.yaml");
    f << "verbose: true\n";
    f << "poll_interval: 10\n";
    f << "max_request_rate: 20\n";
    f << "log_level: debug\n";
    f.close();
  }
  agpm::Config ycfg = agpm::Config::from_file("config.yaml");
  assert(ycfg.verbose());
  assert(ycfg.poll_interval() == 10);
  assert(ycfg.max_request_rate() == 20);
  assert(ycfg.log_level() == "debug");

  {
    std::ofstream f("config.json");
    f << "{";
    f << "\"verbose\":false,";
    f << "\"poll_interval\":5,";
    f << "\"max_request_rate\":15,";
    f << "\"log_level\":\"warn\"";
    f << "}";
    f.close();
  }
  agpm::Config jcfg = agpm::Config::from_file("config.json");
  assert(!jcfg.verbose());
  assert(jcfg.poll_interval() == 5);
  assert(jcfg.max_request_rate() == 15);
  assert(jcfg.log_level() == "warn");

  return 0;
}
