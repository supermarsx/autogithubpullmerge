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
    f << "download_limit: 11\n";
    f << "upload_limit: 12\n";
    f << "max_download: 13\n";
    f << "max_upload: 14\n";
    f.close();
  }
  agpm::Config ycfg = agpm::Config::from_file("config.yaml");
  assert(ycfg.verbose());
  assert(ycfg.poll_interval() == 10);
  assert(ycfg.max_request_rate() == 20);
  assert(ycfg.log_level() == "debug");
  assert(ycfg.http_timeout() == 60);
  assert(ycfg.http_retries() == 7);
  assert(ycfg.download_limit() == 11);
  assert(ycfg.upload_limit() == 12);
  assert(ycfg.max_download() == 13);
  assert(ycfg.max_upload() == 14);

  {
    std::ofstream f("config.json");
    f << "{";
    f << "\"verbose\":false,";
    f << "\"poll_interval\":5,";
    f << "\"max_request_rate\":15,";
    f << "\"log_level\":\"warn\",";
    f << "\"http_timeout\":50,";
    f << "\"http_retries\":4,";
    f << "\"download_limit\":21,";
    f << "\"upload_limit\":22,";
    f << "\"max_download\":23,";
    f << "\"max_upload\":24";
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
  assert(jcfg.download_limit() == 21);
  assert(jcfg.upload_limit() == 22);
  assert(jcfg.max_download() == 23);
  assert(jcfg.max_upload() == 24);

  return 0;
}
