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
    f << "http_timeout: 60\n";
    f << "http_retries: 7\n";
    f.close();
  }
  agpm::Config ycfg = agpm::Config::from_file("config.yaml");
  assert(ycfg.verbose());
  assert(ycfg.poll_interval() == 10);
  assert(ycfg.max_request_rate() == 20);
  assert(ycfg.log_level() == "debug");
  assert(ycfg.http_timeout() == 60);
  assert(ycfg.http_retries() == 7);

  {
    std::ofstream f("config.json");
    f << "{";
    f << "\"verbose\":false,";
    f << "\"poll_interval\":5,";
    f << "\"max_request_rate\":15,";
    f << "\"log_level\":\"warn\",";
    f << "\"http_timeout\":50,";
    f << "\"http_retries\":4";
    f << "}";
    f.close();
  }
  agpm::Config jcfg = agpm::Config::from_file("config.json");
  assert(!jcfg.verbose());
  assert(jcfg.poll_interval() == 5);
  assert(jcfg.max_request_rate() == 15);
  assert(jcfg.log_level() == "warn");
  assert(jcfg.http_timeout() == 50);
  assert(jcfg.http_retries() == 4);

  return 0;
}
