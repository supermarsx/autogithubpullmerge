#include "config_manager.hpp"
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

TEST_CASE("test config manager") {
  agpm::ConfigManager mgr;
  {
    std::ofstream f("cfg.yaml");
    f << "core:\n";
    f << "  verbose: true\n";
    f << "  poll_interval: 4\n";
    f << "rate_limits:\n";
    f << "  max_request_rate: 7\n";
    f << "  max_hourly_requests: 1400\n";
    f << "logging:\n";
    f << "  log_level: info\n";
    f.close();
  }
  agpm::Config yaml_cfg = mgr.load("cfg.yaml");
  REQUIRE(yaml_cfg.verbose());
  REQUIRE(yaml_cfg.poll_interval() == 4);
  REQUIRE(yaml_cfg.max_request_rate() == 7);
  REQUIRE(yaml_cfg.max_hourly_requests() == 1400);
  REQUIRE(yaml_cfg.log_level() == "info");

  {
    nlohmann::json doc;
    doc["core"] = {{"verbose", false}, {"poll_interval", 1}};
    doc["rate_limits"] = {{"max_request_rate", 3},
                          {"max_hourly_requests", 1600}};
    doc["logging"] = {{"log_level", "error"}};
    std::ofstream f("cfg.json");
    f << doc.dump();
  }
  agpm::Config json_cfg = mgr.load("cfg.json");
  REQUIRE(!json_cfg.verbose());
  REQUIRE(json_cfg.poll_interval() == 1);
  REQUIRE(json_cfg.max_request_rate() == 3);
  REQUIRE(json_cfg.max_hourly_requests() == 1600);
  REQUIRE(json_cfg.log_level() == "error");

  {
    std::ofstream f("cfg.toml");
    f << "[core]\n";
    f << "verbose = true\n";
    f << "poll_interval = 9\n\n";

    f << "[rate_limits]\n";
    f << "max_request_rate = 11\n";
    f << "max_hourly_requests = 1800\n\n";

    f << "[logging]\n";
    f << "log_level = \"debug\"\n";
    f.close();
  }
  agpm::Config toml_cfg = mgr.load("cfg.toml");
  REQUIRE(toml_cfg.verbose());
  REQUIRE(toml_cfg.poll_interval() == 9);
  REQUIRE(toml_cfg.max_request_rate() == 11);
  REQUIRE(toml_cfg.max_hourly_requests() == 1800);
  REQUIRE(toml_cfg.log_level() == "debug");
}
