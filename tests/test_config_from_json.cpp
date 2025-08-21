#include "config.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <nlohmann/json.hpp>

TEST_CASE("test config from json") {
  nlohmann::json j;
  j["verbose"] = true;
  j["poll_interval"] = 12;
  j["max_request_rate"] = 42;
  j["log_level"] = "debug";
  j["include_repos"] = {"a", "b"};
  j["pr_since"] = "5m";
  j["http_timeout"] = 40;
  j["http_retries"] = 5;
  j["download_limit"] = 123;
  j["upload_limit"] = 456;
  j["max_download"] = 789;
  j["max_upload"] = 1011;

  agpm::Config cfg = agpm::Config::from_json(j);

  REQUIRE(cfg.verbose());
  REQUIRE(cfg.poll_interval() == 12);
  REQUIRE(cfg.max_request_rate() == 42);
  REQUIRE(cfg.log_level() == "debug");
  REQUIRE(cfg.include_repos().size() == 2);
  REQUIRE(cfg.pr_since() == std::chrono::minutes(5));
  REQUIRE(cfg.http_timeout() == 40);
  REQUIRE(cfg.http_retries() == 5);
  REQUIRE(cfg.download_limit() == 123);
  REQUIRE(cfg.upload_limit() == 456);
  REQUIRE(cfg.max_download() == 789);
  REQUIRE(cfg.max_upload() == 1011);
}
