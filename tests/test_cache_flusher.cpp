#include "github_client.hpp"
#include <catch2/catch_test_macros.hpp>
#include <fstream>

using namespace agpm;

TEST_CASE("cache flusher persists cache") {
  std::string tmpfile = "test_cache_flusher.json";
  // Remove any pre-existing file
  std::remove(tmpfile.c_str());

  class FakeClient : public HttpClient {
  public:
    HttpResponse get_with_headers(const std::string &,
                                  const std::vector<std::string> &) override {
      return {"[]", {"ETag: abc123"}, 200};
    }
    std::string get(const std::string &,
                    const std::vector<std::string> &) override {
      return "";
    }
    std::string put(const std::string &, const std::string &,
                    const std::vector<std::string> &) override {
      return "";
    }
    std::string del(const std::string &,
                    const std::vector<std::string> &) override {
      return "";
    }
  };

  auto http = std::make_unique<FakeClient>();
  GitHubClient client({"tok"}, std::move(http), {}, {}, 0, 30000, 0,
                      "https://api.github.com", false, tmpfile);
  // Trigger a cache update: call list_pull_requests so URL used by caching is
  // exercised
  client.list_pull_requests("me", "repo");
  // Force flush
  client.flush_cache();
  std::ifstream in(tmpfile);
  REQUIRE(in.good());
  nlohmann::json j;
  in >> j;
  REQUIRE(!j.empty());
  std::remove(tmpfile.c_str());
}
