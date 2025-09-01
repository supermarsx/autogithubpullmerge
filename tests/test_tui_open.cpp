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
#include <string>

using namespace agpm;

class MockHttpClient : public HttpClient {
public:
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
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
    return "{}";
  }
};

TEST_CASE("tui open pr") {
#ifdef _WIN32
  _putenv_s("TERM", "xterm");
#else
  setenv("TERM", "xterm", 1);
#endif

  auto mock = std::make_unique<MockHttpClient>();
  GitHubClient client({"token"}, std::move(mock));
  GitHubPoller poller(client, {{"o", "r"}}, 1000, 60);
  Tui ui(client, poller);
  ui.init();
  if (!ui.initialized()) {
    WARN("Skipping TUI test: no TTY available");
    return;
  }
  ui.update_prs({{1, "PR", false, "o", "r"}});
  std::string opened;
  ui.set_open_cmd([&](const std::string &url) {
    opened = url;
    return 0;
  });
  ui.handle_key('o');
  REQUIRE(opened == "https://github.com/o/r/pull/1");
  ui.cleanup();
}
