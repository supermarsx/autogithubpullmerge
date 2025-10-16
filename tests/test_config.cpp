#include "config.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <fstream>

TEST_CASE("test config") {
  // YAML config with extended options
  {
    std::ofstream f("cfg.yaml");
    f << "verbose: true\n";
    f << "poll_interval: 3\n";
    f << "max_request_rate: 10\n";
    f << "log_level: debug\n";
    f << "include_repos:\n  - repoA\n  - repoB\n";
    f << "exclude_repos:\n  - repoC\n";
    f << "api_keys:\n  - a\n  - b\n";
    f << "include_merged: true\n";
    f << "history_db: hist.db\n";
    f << "only_poll_prs: true\n";
    f << "reject_dirty: true\n";
    f << "auto_merge: true\n";
    f << "purge_only: true\n";
    f << "purge_prefix: tmp/\n";
    f << "pr_limit: 25\n";
    f << "pr_since: 2h\n";
    f << "sort: reverse\n";
    f << "download_limit: 1000\n";
    f << "upload_limit: 2000\n";
    f << "max_download: 3000\n";
    f << "max_upload: 4000\n";
    f << "http_proxy: http://proxy\n";
    f << "https_proxy: http://secureproxy\n";
    f.close();
  }
  agpm::Config yaml_cfg = agpm::Config::from_file("cfg.yaml");
  REQUIRE(yaml_cfg.verbose());
  REQUIRE(yaml_cfg.poll_interval() == 3);
  REQUIRE(yaml_cfg.max_request_rate() == 10);
  REQUIRE(yaml_cfg.log_level() == "debug");
  REQUIRE(yaml_cfg.include_repos().size() == 2);
  REQUIRE(yaml_cfg.include_repos()[0] == "repoA");
  REQUIRE(yaml_cfg.exclude_repos().size() == 1);
  REQUIRE(yaml_cfg.exclude_repos()[0] == "repoC");
  REQUIRE(yaml_cfg.api_keys().size() == 2);
  REQUIRE(yaml_cfg.include_merged());
  REQUIRE(yaml_cfg.history_db() == "hist.db");
  REQUIRE(yaml_cfg.only_poll_prs());
  REQUIRE(yaml_cfg.reject_dirty());
  REQUIRE(yaml_cfg.auto_merge());
  REQUIRE(yaml_cfg.purge_only());
  REQUIRE(yaml_cfg.purge_prefix() == "tmp/");
  REQUIRE(yaml_cfg.pr_limit() == 25);
  REQUIRE(yaml_cfg.pr_since() == std::chrono::hours(2));
  REQUIRE(yaml_cfg.sort_mode() == "reverse");
  REQUIRE(yaml_cfg.download_limit() == 1000);
  REQUIRE(yaml_cfg.upload_limit() == 2000);
  REQUIRE(yaml_cfg.max_download() == 3000);
  REQUIRE(yaml_cfg.max_upload() == 4000);
  REQUIRE(yaml_cfg.http_proxy() == "http://proxy");
  REQUIRE(yaml_cfg.https_proxy() == "http://secureproxy");

  // JSON config with extended options
  {
    std::ofstream f("cfg.json");
    f << "{";
    f << "\"verbose\": false,";
    f << "\"poll_interval\":2,";
    f << "\"max_request_rate\":5,";
    f << "\"log_level\":\"warn\",";
    f << "\"include_repos\":[\"x\"],";
    f << "\"exclude_repos\":[\"y\",\"z\"],";
    f << "\"api_keys\":[\"k1\"],";
    f << "\"history_db\":\"db.sqlite\",";
    f << "\"only_poll_stray\":true,";
    f << "\"purge_only\":true,";
    f << "\"purge_prefix\":\"test/\",";
    f << "\"pr_limit\":30,";
    f << "\"pr_since\":\"15m\",";
    f << "\"sort\":\"alphanum\",";
    f << "\"download_limit\":500,";
    f << "\"upload_limit\":600,";
    f << "\"max_download\":700,";
    f << "\"max_upload\":800,";
    f << "\"http_proxy\":\"http://proxy\",";
    f << "\"https_proxy\":\"http://secureproxy\"";
    f << "}";
    f.close();
  }
  agpm::Config json_cfg = agpm::Config::from_file("cfg.json");
  REQUIRE(!json_cfg.verbose());
  REQUIRE(json_cfg.poll_interval() == 2);
  REQUIRE(json_cfg.max_request_rate() == 5);
  REQUIRE(json_cfg.log_level() == "warn");
  REQUIRE(json_cfg.include_repos().size() == 1);
  REQUIRE(json_cfg.include_repos()[0] == "x");
  REQUIRE(json_cfg.exclude_repos().size() == 2);
  REQUIRE(json_cfg.exclude_repos()[1] == "z");
  REQUIRE(json_cfg.api_keys().size() == 1);
  REQUIRE(json_cfg.history_db() == "db.sqlite");
  REQUIRE(json_cfg.only_poll_stray());
  REQUIRE(json_cfg.purge_only());
  REQUIRE(json_cfg.purge_prefix() == "test/");
  REQUIRE(json_cfg.pr_limit() == 30);
  REQUIRE(json_cfg.pr_since() == std::chrono::minutes(15));
  REQUIRE(json_cfg.sort_mode() == "alphanum");
  REQUIRE(json_cfg.download_limit() == 500);
  REQUIRE(json_cfg.upload_limit() == 600);
  REQUIRE(json_cfg.max_download() == 700);
  REQUIRE(json_cfg.max_upload() == 800);
  REQUIRE(json_cfg.http_proxy() == "http://proxy");
  REQUIRE(json_cfg.https_proxy() == "http://secureproxy");

  // TOML config with extended options
  {
    std::ofstream f("cfg.toml");
    f << "verbose = true\n";
    f << "poll_interval = 8\n";
    f << "max_request_rate = 12\n";
    f << "log_level = \"info\"\n";
    f << "include_repos = [\"repoTomlA\", \"repoTomlB\"]\n";
    f << "exclude_repos = [\"repoTomlC\"]\n";
    f << "api_keys = [\"tok1\", \"tok2\"]\n";
    f << "history_db = \"history_toml.db\"\n";
    f << "only_poll_stray = true\n";
    f << "purge_only = false\n";
    f << "purge_prefix = \"hotfix/\"\n";
    f << "pr_limit = 15\n";
    f << "pr_since = \"45m\"\n";
    f << "sort = \"reverse-alphanum\"\n";
    f << "download_limit = 1500\n";
    f << "upload_limit = 1600\n";
    f << "max_download = 1700\n";
    f << "max_upload = 1800\n";
    f << "http_timeout = 45\n";
    f << "http_retries = 6\n";
    f << "use_graphql = true\n";
    f.close();
  }
  agpm::Config toml_cfg = agpm::Config::from_file("cfg.toml");
  REQUIRE(toml_cfg.verbose());
  REQUIRE(toml_cfg.poll_interval() == 8);
  REQUIRE(toml_cfg.max_request_rate() == 12);
  REQUIRE(toml_cfg.log_level() == "info");
  REQUIRE(toml_cfg.include_repos().size() == 2);
  REQUIRE(toml_cfg.include_repos()[1] == "repoTomlB");
  REQUIRE(toml_cfg.exclude_repos().size() == 1);
  REQUIRE(toml_cfg.exclude_repos()[0] == "repoTomlC");
  REQUIRE(toml_cfg.api_keys().size() == 2);
  REQUIRE(toml_cfg.history_db() == "history_toml.db");
  REQUIRE(toml_cfg.only_poll_stray());
  REQUIRE(!toml_cfg.purge_only());
  REQUIRE(toml_cfg.purge_prefix() == "hotfix/");
  REQUIRE(toml_cfg.pr_limit() == 15);
  REQUIRE(toml_cfg.pr_since() == std::chrono::minutes(45));
  REQUIRE(toml_cfg.sort_mode() == "reverse-alphanum");
  REQUIRE(toml_cfg.download_limit() == 1500);
  REQUIRE(toml_cfg.upload_limit() == 1600);
  REQUIRE(toml_cfg.max_download() == 1700);
  REQUIRE(toml_cfg.max_upload() == 1800);
  REQUIRE(toml_cfg.http_timeout() == 45);
  REQUIRE(toml_cfg.http_retries() == 6);
  REQUIRE(toml_cfg.use_graphql());
}
