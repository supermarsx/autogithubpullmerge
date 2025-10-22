#include "config.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <nlohmann/json.hpp>

TEST_CASE("test config from json") {
  nlohmann::json j;
  auto &core = j["core"];
  core["verbose"] = true;
  core["poll_interval"] = 12;
  core["use_graphql"] = true;

  auto &limits = j["rate_limits"];
  limits["max_request_rate"] = 42;
  limits["max_hourly_requests"] = 3600;

  auto &logging = j["logging"];
  logging["log_level"] = "debug";
  logging["log_limit"] = 210;
  logging["log_rotate"] = 6;
  logging["log_compress"] = true;
  logging["log_categories"] = {{"history", "trace"}, {"http", "debug"}};

  auto &repos = j["repositories"];
  repos["include_repos"] = {"a", "b"};

  auto &workflow = j["workflow"];
  workflow["pr_since"] = "5m";
  workflow["assume_yes"] = true;
  workflow["dry_run"] = true;
  workflow["delete_stray"] = true;
  workflow["allow_delete_base_branch"] = true;

  auto &network = j["network"];
  network["http_timeout"] = 40;
  network["http_retries"] = 5;
  network["download_limit"] = 123;
  network["upload_limit"] = 456;
  network["max_download"] = 789;
  network["max_upload"] = 1011;
  network["http_proxy"] = "http://proxy";
  network["https_proxy"] = "http://secureproxy";

  auto &artifacts = j["artifacts"];
  artifacts["export_csv"] = "cfg.csv";
  artifacts["export_json"] = "cfg.json";

  auto &pat = j["personal_access_tokens"];
  pat["open_pat_page"] = false;
  pat["pat_save_path"] = "cfg_pat.txt";
  pat["pat_value"] = "cfg_pat_value";

  auto &single = j["single_run"];
  single["single_open_prs_repo"] = "cfg/open";
  single["single_branches_repo"] = "cfg/branches";

  auto &ui = j["ui"];
  ui["hotkeys"] = {{"enabled", false},
                    {"bindings",
                     {{"refresh", nlohmann::json::array({"Ctrl+R", "r"})},
                      {"merge", nullptr},
                      {"details", "enter"}}}};

  agpm::Config cfg = agpm::Config::from_json(j);

  REQUIRE(cfg.verbose());
  REQUIRE(cfg.poll_interval() == 12);
  REQUIRE(cfg.max_request_rate() == 42);
  REQUIRE(cfg.max_hourly_requests() == 3600);
  REQUIRE(cfg.log_level() == "debug");
  REQUIRE(cfg.log_limit() == 210);
  REQUIRE(cfg.log_rotate() == 6);
  REQUIRE(cfg.log_compress());
  REQUIRE(cfg.log_categories().at("history") == "trace");
  REQUIRE(cfg.log_categories().at("http") == "debug");
  REQUIRE(cfg.include_repos().size() == 2);
  REQUIRE(cfg.pr_since() == std::chrono::minutes(5));
  REQUIRE(cfg.http_timeout() == 40);
  REQUIRE(cfg.http_retries() == 5);
  REQUIRE(cfg.download_limit() == 123);
  REQUIRE(cfg.upload_limit() == 456);
  REQUIRE(cfg.max_download() == 789);
  REQUIRE(cfg.max_upload() == 1011);
  REQUIRE(cfg.http_proxy() == "http://proxy");
  REQUIRE(cfg.https_proxy() == "http://secureproxy");
  REQUIRE(cfg.use_graphql());
  REQUIRE(cfg.assume_yes());
  REQUIRE(cfg.dry_run());
  REQUIRE(cfg.delete_stray());
  REQUIRE(cfg.allow_delete_base_branch());
  REQUIRE(cfg.export_csv() == "cfg.csv");
  REQUIRE(cfg.export_json() == "cfg.json");
  REQUIRE_FALSE(cfg.open_pat_page());
  REQUIRE(cfg.pat_save_path() == "cfg_pat.txt");
  REQUIRE(cfg.pat_value() == "cfg_pat_value");
  REQUIRE(cfg.single_open_prs_repo() == "cfg/open");
  REQUIRE(cfg.single_branches_repo() == "cfg/branches");
  REQUIRE_FALSE(cfg.hotkeys_enabled());
  REQUIRE(cfg.hotkey_bindings().at("refresh") == "Ctrl+R,r");
  REQUIRE(cfg.hotkey_bindings().at("merge").empty());
  REQUIRE(cfg.hotkey_bindings().at("details") == "enter");
}
