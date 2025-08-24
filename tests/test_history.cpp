#include "cli.hpp"
#include "github_poller.hpp"
#include "history.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <fstream>

using namespace agpm;

class DummyHttpClient : public HttpClient {
public:
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return "[{\"number\":1,\"title\":\"Test PR\"}]";
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

TEST_CASE("test history") {
  auto http = std::make_unique<DummyHttpClient>();
  GitHubClient client({"tok"}, std::unique_ptr<HttpClient>(http.release()));
  PullRequestHistory hist("test_history.db");

  char prog[] = "tests";
  char csv_flag[] = "--export-csv";
  char csv_path[] = "out.csv";
  char json_flag[] = "--export-json";
  char json_path[] = "out.json";
  char *argv[] = {prog, csv_flag, csv_path, json_flag, json_path};
  CliOptions opts = parse_cli(5, argv);

  GitHubPoller poller(client, {{"me", "repo"}}, 0, 120, false, false, false, "",
                      false, false, "", &hist);

  if (!opts.export_csv.empty() || !opts.export_json.empty()) {
    poller.set_export_callback([&]() {
      if (!opts.export_csv.empty()) {
        hist.export_csv(opts.export_csv);
      }
      if (!opts.export_json.empty()) {
        hist.export_json(opts.export_json);
      }
    });
  }
  poller.poll_now();

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
