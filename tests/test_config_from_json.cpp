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
  j["log_limit"] = 210;
  j["log_rotate"] = 6;
  j["log_compress"] = true;
  j["log_categories"] = {{"history", "trace"}, {"http", "debug"}};
  j["include_repos"] = {"a", "b"};
  j["pr_since"] = "5m";
  j["http_timeout"] = 40;
  j["http_retries"] = 5;
  j["download_limit"] = 123;
  j["upload_limit"] = 456;
  j["max_download"] = 789;
  j["max_upload"] = 1011;
  j["http_proxy"] = "http://proxy";
  j["https_proxy"] = "http://secureproxy";
  j["use_graphql"] = true;
  j["assume_yes"] = true;
  j["dry_run"] = true;
  j["delete_stray"] = true;
  j["allow_delete_base_branch"] = true;
  j["export_csv"] = "cfg.csv";
  j["export_json"] = "cfg.json";
  j["open_pat_page"] = false;
  j["pat_save_path"] = "cfg_pat.txt";
  j["pat_value"] = "cfg_pat_value";
  j["single_open_prs_repo"] = "cfg/open";
  j["single_branches_repo"] = "cfg/branches";
  j["hotkeys"] = {{"enabled", false},
                  {"bindings",
                   {{"refresh", nlohmann::json::array({"Ctrl+R", "r"})},
                    {"merge", nullptr},
                    {"details", "enter"}}}};

  agpm::Config cfg = agpm::Config::from_json(j);

  REQUIRE(cfg.verbose());
  REQUIRE(cfg.poll_interval() == 12);
  REQUIRE(cfg.max_request_rate() == 42);
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
