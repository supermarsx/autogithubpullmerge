#include "github_poller.hpp"
#define private public
#include "tui.hpp"
#undef private
#include <array>
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

TEST_CASE("tui show details") {
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
  if (!ui.initialized_) {
    WARN("Skipping TUI test: no TTY available");
    return;
  }
  ui.update_prs({{1, "PR title", false, "o", "r"}});
  ui.handle_key('d');
  ui.draw();
  std::array<char, 80> buf{};
  mvwinnstr(ui.detail_win_, 2, 1, buf.data(), 79);
  std::string detail(buf.data());
  REQUIRE(detail.find("PR title") != std::string::npos);
  ui.handle_key('d');
  ui.cleanup();
}

TEST_CASE("tui show details enter") {
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
  if (!ui.initialized_) {
    WARN("Skipping TUI test: no TTY available");
    return;
  }
  ui.update_prs({{2, "Another", false, "o", "r"}});
  ui.handle_key('\n');
  ui.draw();
  std::array<char, 80> buf2{};
  mvwinnstr(ui.detail_win_, 2, 1, buf2.data(), 79);
  std::string detail2(buf2.data());
  REQUIRE(detail2.find("Another") != std::string::npos);
  ui.handle_key('\n');
  ui.cleanup();
}
