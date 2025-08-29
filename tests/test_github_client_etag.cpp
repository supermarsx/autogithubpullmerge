#include "github_client.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

using namespace agpm;

class ETagHttpClient : public HttpClient {
public:
  std::vector<HttpResponse> responses;
  std::vector<std::vector<std::string>> seen_headers;
  size_t index{0};

  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    return get_with_headers(url, headers).body;
  }

  HttpResponse
  get_with_headers(const std::string &url,
                   const std::vector<std::string> &headers) override {
    (void)url;
    seen_headers.push_back(headers);
    if (index < responses.size()) {
      return responses[index++];
    }
    return {};
  }

  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)data;
    (void)headers;
    return {};
  }

  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return {};
  }
};

TEST_CASE("github client caches etags and persists") {
  ETagHttpClient *raw = nullptr;
  std::filesystem::path cache =
      std::filesystem::temp_directory_path() / "agpm_etag_cache.json";
  {
    auto http = std::make_unique<ETagHttpClient>();
    raw = http.get();
    HttpResponse first{
        R"([{"number":1,"title":"t","created_at":"2021-01-01T00:00:00Z"}])",
        {"ETag: abc"},
        200};
    HttpResponse not_modified{"", {}, 304};
    raw->responses = {first, not_modified};
    GitHubClient client({"tok"}, std::move(http), {}, {}, 0, 30000, 3,
                        "https://api.github.com", false, cache.string());
    auto prs1 = client.list_pull_requests("o", "r");
    REQUIRE(prs1.size() == 1);
    auto prs2 = client.list_pull_requests("o", "r");
    REQUIRE(prs2.size() == 1);
    bool sent_if_none = false;
    for (const auto &h : raw->seen_headers[1]) {
      if (h == "If-None-Match: abc")
        sent_if_none = true;
    }
    REQUIRE(sent_if_none);
  }

  auto http2 = std::make_unique<ETagHttpClient>();
  ETagHttpClient *raw2 = http2.get();
  HttpResponse nm{"", {}, 304};
  raw2->responses = {nm};
  GitHubClient client2({"tok"}, std::move(http2), {}, {}, 0, 30000, 3,
                       "https://api.github.com", false, cache.string());
  auto prs3 = client2.list_pull_requests("o", "r");
  REQUIRE(prs3.size() == 1);
  bool sent_if_none2 = false;
  for (const auto &h : raw2->seen_headers[0]) {
    if (h == "If-None-Match: abc")
      sent_if_none2 = true;
  }
  REQUIRE(sent_if_none2);
  std::filesystem::remove(cache);
}
