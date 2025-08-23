#include "github_client.hpp"
#include "history.hpp"
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <vector>

using namespace agpm;

class DummyHttp : public HttpClient {
public:
  std::string resp_list;
  std::string resp_pr;
  std::vector<std::string> resp_puts;
  size_t put_index = 0;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    if (url.find("/pulls/") != std::string::npos) {
      return resp_pr;
    }
    return resp_list;
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    (void)headers;
    if (put_index < resp_puts.size()) {
      return resp_puts[put_index++];
    }
    return "{}";
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return "";
  }
};

TEST_CASE("test history merge") {
  auto http = std::make_unique<DummyHttp>();
  http->resp_list =
      "[{\"number\":1,\"title\":\"One\"},{\"number\":2,\"title\":\"Two\"}]";
  http->resp_pr = "{}";
  http->resp_puts = {"{\"merged\":true}", "{\"merged\":false}"};
  DummyHttp *raw = http.get();
  (void)raw;
  GitHubClient client("tok", std::unique_ptr<HttpClient>(http.release()));
  auto prs = client.list_pull_requests("me", "repo");
  PullRequestHistory hist("merge_test.db");
  for (const auto &pr : prs) {
    REQUIRE(pr.owner == "me");
    REQUIRE(pr.repo == "repo");
    hist.insert(pr.number, pr.title, pr.merged);
    bool merged = client.merge_pull_request(pr.owner, pr.repo, pr.number);
    if (merged) {
      hist.update_merged(pr.number);
    }
  }
  hist.export_json("merge.json");
  std::ifstream f("merge.json");
  nlohmann::json j;
  f >> j;
  REQUIRE(j.size() == 2);
  REQUIRE(j[0]["number"] == 1);
  REQUIRE(j[0]["title"] == "One");
  REQUIRE(j[0]["merged"] == true);
  REQUIRE(j[1]["merged"] == false);
  std::remove("merge_test.db");
  std::remove("merge.json");
}
