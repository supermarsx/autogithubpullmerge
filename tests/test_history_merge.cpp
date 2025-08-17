#include "github_client.hpp"
#include "history.hpp"
#include <cassert>
#include <fstream>

using namespace agpm;

class DummyHttp : public HttpClient {
public:
  std::string resp_get;
  std::string resp_put;
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return resp_get;
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    (void)headers;
    return resp_put;
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return "";
  }
};

int main() {
  auto http = std::make_unique<DummyHttp>();
  http->resp_get =
      "[{\"number\":1,\"title\":\"One\"},{\"number\":2,\"title\":\"Two\"}]";
  http->resp_put = "{\"merged\":true}";
  DummyHttp *raw = http.get();
  (void)raw;
  GitHubClient client("tok", std::unique_ptr<HttpClient>(http.release()));
  auto prs = client.list_pull_requests("me", "repo");
  PullRequestHistory hist("merge_test.db");
  for (const auto &pr : prs) {
    assert(pr.owner == "me");
    assert(pr.repo == "repo");
    bool merged = client.merge_pull_request(pr.owner, pr.repo, pr.number);
    hist.insert(pr.number, pr.title, merged);
  }
  hist.export_json("merge.json");
  std::ifstream f("merge.json");
  nlohmann::json j;
  f >> j;
  assert(j.size() == 2);
  assert(j[0]["number"] == 1);
  assert(j[0]["title"] == "One");
  assert(j[0]["merged"] == true);
  std::remove("merge_test.db");
  std::remove("merge.json");
  return 0;
}
