#include "config.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

TEST_CASE("test config loading") {
  {
    std::filesystem::path ypath =
        std::filesystem::temp_directory_path() / "agpm_cfg.yaml";
    std::ofstream f(ypath.string());
    f << "core:\n";
    f << "  verbose: true\n";
    f << "  poll_interval: 10\n";
    f << "rate_limits:\n";
    f << "  max_request_rate: 20\n";
    f << "  max_hourly_requests: 2000\n";
    f << "logging:\n";
    f << "  log_level: debug\n";
    f << "network:\n";
    f << "  http_timeout: 60\n";
    f << "  http_retries: 7\n";
    f << "  download_limit: 11\n";
    f << "  upload_limit: 12\n";
    f << "  max_download: 13\n";
    f << "  max_upload: 14\n";
    f << "  http_proxy: http://proxy\n";
    f << "  https_proxy: http://secureproxy\n";
    f << "features:\n";
    f << "  use_graphql: true\n";
    f.close();
  }
  agpm::Config ycfg = agpm::Config::from_file(
      (std::filesystem::temp_directory_path() / "agpm_cfg.yaml").string());
  REQUIRE(ycfg.verbose());
  REQUIRE(ycfg.poll_interval() == 10);
  REQUIRE(ycfg.max_request_rate() == 20);
  REQUIRE(ycfg.max_hourly_requests() == 2000);
  REQUIRE(ycfg.log_level() == "debug");
  REQUIRE(ycfg.http_timeout() == 60);
  REQUIRE(ycfg.http_retries() == 7);
  REQUIRE(ycfg.download_limit() == 11);
  REQUIRE(ycfg.upload_limit() == 12);
  REQUIRE(ycfg.max_download() == 13);
  REQUIRE(ycfg.max_upload() == 14);
  REQUIRE(ycfg.http_proxy() == "http://proxy");
  REQUIRE(ycfg.https_proxy() == "http://secureproxy");
  REQUIRE(ycfg.use_graphql());

  {
    std::filesystem::path jpath =
        std::filesystem::temp_directory_path() / "agpm_cfg.json";
    nlohmann::json doc;
    doc["core"] = {{"verbose", false}, {"poll_interval", 5}};
    doc["rate_limits"] = {{"max_request_rate", 15},
                          {"max_hourly_requests", 2500}};
    doc["logging"] = {{"log_level", "warn"}};
    doc["network"] = {
        {"http_timeout", 50},           {"http_retries", 4},
        {"download_limit", 21},         {"upload_limit", 22},
        {"max_download", 23},           {"max_upload", 24},
        {"http_proxy", "http://proxy"}, {"https_proxy", "http://secureproxy"}};
    doc["features"] = {{"use_graphql", false}};
    std::ofstream f(jpath.string());
    f << doc.dump();
  }
  agpm::Config jcfg = agpm::Config::from_file(
      (std::filesystem::temp_directory_path() / "agpm_cfg.json").string());
  REQUIRE(!jcfg.verbose());
  REQUIRE(jcfg.poll_interval() == 5);
  REQUIRE(jcfg.max_request_rate() == 15);
  REQUIRE(jcfg.max_hourly_requests() == 2500);
  REQUIRE(jcfg.log_level() == "warn");
  REQUIRE(jcfg.http_timeout() == 50);
  REQUIRE(jcfg.http_retries() == 4);
  REQUIRE(jcfg.download_limit() == 21);
  REQUIRE(jcfg.upload_limit() == 22);
  REQUIRE(jcfg.max_download() == 23);
  REQUIRE(jcfg.max_upload() == 24);
  REQUIRE(jcfg.http_proxy() == "http://proxy");
  REQUIRE(jcfg.https_proxy() == "http://secureproxy");
  REQUIRE(!jcfg.use_graphql());

  {
    std::filesystem::path tpath =
        std::filesystem::temp_directory_path() / "agpm_cfg.toml";
    std::ofstream f(tpath.string());
    f << "[core]\n";
    f << "verbose = false\n";
    f << "poll_interval = 12\n\n";

    f << "[rate_limits]\n";
    f << "max_request_rate = 18\n";
    f << "max_hourly_requests = 3000\n\n";

    f << "[logging]\n";
    f << "log_level = \"info\"\n\n";

    f << "[network]\n";
    f << "http_timeout = 40\n";
    f << "http_retries = 5\n";
    f << "download_limit = 31\n";
    f << "upload_limit = 32\n";
    f << "max_download = 33\n";
    f << "max_upload = 34\n";
    f << "http_proxy = \"http://proxy\"\n";
    f << "https_proxy = \"http://secureproxy\"\n\n";

    f << "[features]\n";
    f << "use_graphql = true\n";
    f.close();
  }
  agpm::Config tcfg = agpm::Config::from_file(
      (std::filesystem::temp_directory_path() / "agpm_cfg.toml").string());
  REQUIRE(!tcfg.verbose());
  REQUIRE(tcfg.poll_interval() == 12);
  REQUIRE(tcfg.max_request_rate() == 18);
  REQUIRE(tcfg.max_hourly_requests() == 3000);
  REQUIRE(tcfg.log_level() == "info");
  REQUIRE(tcfg.http_timeout() == 40);
  REQUIRE(tcfg.http_retries() == 5);
  REQUIRE(tcfg.download_limit() == 31);
  REQUIRE(tcfg.upload_limit() == 32);
  REQUIRE(tcfg.max_download() == 33);
  REQUIRE(tcfg.max_upload() == 34);
  REQUIRE(tcfg.http_proxy() == "http://proxy");
  REQUIRE(tcfg.https_proxy() == "http://secureproxy");
  REQUIRE(tcfg.use_graphql());
}
