#include "config_manager.hpp"
#include <catch2/catch_test_macros.hpp>
#include <fstream>

TEST_CASE("test config manager") {
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
  REQUIRE(yaml_cfg.verbose());
  REQUIRE(yaml_cfg.poll_interval() == 4);
  REQUIRE(yaml_cfg.max_request_rate() == 7);
  REQUIRE(yaml_cfg.log_level() == "info");

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
  REQUIRE(!json_cfg.verbose());
  REQUIRE(json_cfg.poll_interval() == 1);
  REQUIRE(json_cfg.max_request_rate() == 3);
  REQUIRE(json_cfg.log_level() == "error");

  {
    std::ofstream f("cfg.toml");
    f << "verbose = true\n";
    f << "poll_interval = 9\n";
    f << "max_request_rate = 11\n";
    f << "log_level = \"debug\"\n";
    f.close();
  }
  agpm::Config toml_cfg = mgr.load("cfg.toml");
  REQUIRE(toml_cfg.verbose());
  REQUIRE(toml_cfg.poll_interval() == 9);
  REQUIRE(toml_cfg.max_request_rate() == 11);
  REQUIRE(toml_cfg.log_level() == "debug");
}
