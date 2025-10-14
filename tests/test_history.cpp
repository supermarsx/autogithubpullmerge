#include "cli.hpp"
#include "github_poller.hpp"
#include "history.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <fstream>
#include <iostream>

using namespace agpm;

namespace {

class DummyHttpClient : public HttpClient {
public:
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    std::cerr << "[Dummy] GET " << url << std::endl;
    if (url.find("/pulls") != std::string::npos) {
      return "[{\"number\":1,\"title\":\"Test PR\"}]";
    }
    if (url.find("/branches") != std::string::npos) {
      return "[]";
    }
    if (url.find("/compare") != std::string::npos) {
      return "{\"status\":\"identical\"}";
    }
    if (url.find("/repos/") != std::string::npos) {
      return "{\"default_branch\":\"main\"}";
    }
    return "{}";
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    (void)headers;
    return "{}";
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return "";
  }
};

} // namespace

TEST_CASE("test history") {
  // Exercise the PullRequestHistory storage and export paths deterministically
  // without depending on GitHubClient/Poller integration.
  PullRequestHistory hist("test_history.db");

  // Insert a single record and export via both formats
  hist.insert(1, "Test PR", false);
  hist.export_csv("out.csv");
  hist.export_json("out.json");

  std::ifstream csv("out.csv");
  std::string line;
  std::getline(csv, line);
  REQUIRE(line == "number,title,merged");
  std::getline(csv, line);
  REQUIRE(line.find("1") != std::string::npos);
  REQUIRE(line.find("Test PR") != std::string::npos);
  csv.close();

  std::ifstream js("out.json");
  nlohmann::json j;
  js >> j;
  REQUIRE(j.is_array());
  REQUIRE(j.size() == 1);
  REQUIRE(j[0]["number"] == 1);
  REQUIRE(j[0]["title"] == "Test PR");
  REQUIRE(j[0]["merged"] == false);
  std::remove("test_history.db");
  std::remove("out.csv");
  std::remove("out.json");
}
