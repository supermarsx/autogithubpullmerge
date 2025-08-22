#include "github_client.hpp"
#include "github_poller.hpp"
#include "tui.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <cstdlib>
#if defined(_WIN32)
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif
#include <memory>

using namespace agpm;

class MockHttpClient : public HttpClient {
public:
  int get_count{0};
  std::string get_response;
  std::string put_response;
  std::string last_url;

  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    last_url = url;
    ++get_count;
    return get_response;
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)data;
    (void)headers;
    last_url = url;
    return put_response;
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    return {};
  }
};

TEST_CASE("test tui merge") {
#ifdef _WIN32
  _putenv_s("TERM", "xterm");
#else
  setenv("TERM", "xterm", 1);
#endif

  if (!isatty(fileno(stdout))) {
    WARN("Skipping TUI test: no TTY available");
    return;
  }

  auto mock = std::make_unique<MockHttpClient>();
  mock->get_response = "[{\"number\":1,\"title\":\"PR\"}]";
  mock->put_response = "{\"merged\":true}";
  MockHttpClient *raw = mock.get();
  GitHubClient client("token", std::unique_ptr<HttpClient>(mock.release()));
  GitHubPoller poller(client, {{"o", "r"}}, 1000, 60);
  Tui ui(client, poller);
  ui.init();

  auto prs = client.list_pull_requests("o", "r");
  ui.update_prs(prs);
  ui.handle_key('m');
  REQUIRE(raw->last_url.find("/repos/o/r/pulls/1/merge") != std::string::npos);
  REQUIRE(!ui.logs().empty());
  REQUIRE(ui.logs().back().find("Merged PR #1") != std::string::npos);

  ui.cleanup();
}
