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
  std::string last_method;
  std::string last_url;

  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)headers;
    last_method = "GET";
    last_url = url;
    ++get_count;
    return get_response;
  }
  std::string put(const std::string &url, const std::string &data,
                  const std::vector<std::string> &headers) override {
    (void)data;
    (void)headers;
    last_method = "PUT";
    last_url = url;
    return put_response;
  }
  std::string del(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    last_method = "DELETE";
    return {};
  }
};

TEST_CASE("test tui log limit") {
#ifdef _WIN32
  _putenv_s("TERM", "xterm");
#else
  setenv("TERM", "xterm", 1);
#endif

  auto mock = std::make_unique<MockHttpClient>();
  mock->put_response = "{\"merged\":true}";
  GitHubClient client({"token"}, std::move(mock));
  GitHubPoller poller(client, {{"o", "r"}}, 1000, 60);
  Tui ui(client, poller);
  ui.init();
  if (!ui.initialized()) {
    WARN("Skipping TUI test: no TTY available");
    return;
  }

  for (int i = 0; i < 205; ++i) {
    ui.update_prs({{i, "PR", false, "o", "r"}});
    ui.handle_key('m');
  }

  REQUIRE(ui.logs().size() == 200);
  REQUIRE(ui.logs().front().find("Merged PR #5") != std::string::npos);
  REQUIRE(ui.logs().back().find("Merged PR #204") != std::string::npos);
}
