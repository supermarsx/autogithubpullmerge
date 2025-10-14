#include "config.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

TEST_CASE("test config loading") {
  {
    std::filesystem::path ypath =
        std::filesystem::temp_directory_path() / "agpm_cfg.yaml";
    std::ofstream f(ypath.string());
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
    f << "http_proxy: http://proxy\n";
    f << "https_proxy: http://secureproxy\n";
    f << "use_graphql: true\n";
    f.close();
  }
  agpm::Config ycfg = agpm::Config::from_file(
      (std::filesystem::temp_directory_path() / "agpm_cfg.yaml").string());
  REQUIRE(ycfg.verbose());
  REQUIRE(ycfg.poll_interval() == 10);
  REQUIRE(ycfg.max_request_rate() == 20);
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
    std::ofstream f(jpath.string());
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
    f << "\"max_upload\":24,";
    f << "\"http_proxy\":\"http://proxy\",";
    f << "\"https_proxy\":\"http://secureproxy\",";
    f << "\"use_graphql\":false";
    f << "}";
    f.close();
  }
  agpm::Config jcfg = agpm::Config::from_file(
      (std::filesystem::temp_directory_path() / "agpm_cfg.json").string());
  REQUIRE(!jcfg.verbose());
  REQUIRE(jcfg.poll_interval() == 5);
  REQUIRE(jcfg.max_request_rate() == 15);
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
}
