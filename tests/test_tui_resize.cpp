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
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
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

TEST_CASE("test tui resize", "[tui]") {
#ifdef _WIN32
  _putenv_s("TERM", "xterm");
#else
  setenv("TERM", "xterm", 1);
#endif

  // Skip entirely if not running on a real TTY to avoid ncurses aborts
  if (!isatty(fileno(stdout)) || !isatty(fileno(stdin)) ||
      !isatty(fileno(stderr))) {
    WARN("Skipping TUI test: no TTY available");
    return;
  }

  auto mock = std::make_unique<MockHttpClient>();
  GitHubClient client({"token"}, std::move(mock));
  GitHubPoller poller(client, {{"o", "r"}}, 1000, 60, 0, 1);
  Tui ui(client, poller, 200);
  ui.init();
  if (!ui.initialized()) {
    WARN("Skipping TUI test: no TTY available");
    ui.cleanup();
    return;
  }

  ui.update_prs({{1, "PR", false, "o", "r"}});
  ui.draw();
  const WINDOW *before = ui.pr_win();
  int h, w;
  getmaxyx(stdscr, h, w);
  resize_term(h / 2, w / 2);
  ui.draw();
  REQUIRE(before != ui.pr_win());
  ui.cleanup();
}
