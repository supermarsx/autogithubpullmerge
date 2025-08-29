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
    if (url.find("/pulls/") != std::string::npos) {
      return "{}";
    }
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

TEST_CASE("test tui") {
#ifdef _WIN32
  _putenv_s("TERM", "xterm");
#else
  setenv("TERM", "xterm", 1);
#endif

  auto mock = std::make_unique<MockHttpClient>();
  mock->get_response = "[{\"number\":1,\"title\":\"Test PR\"}]";
  mock->put_response = "{\"merged\":true}";
  MockHttpClient *raw = mock.get();
  GitHubClient client({"token"}, std::unique_ptr<HttpClient>(mock.release()));
  GitHubPoller poller(client, {{"o", "r"}}, 1000, 60);
  Tui ui(client, poller);
  ui.init();
  if (!ui.initialized_) {
    WARN("Skipping TUI test: no TTY available");
    return;
  }

  ui.update_prs({{1, "Test PR", false, "o", "r"}});
  ui.draw();
  std::array<char, 80> buf{};
  mvwinnstr(stdscr, 1, 1, buf.data(), 79);
  std::string line(buf.data());
  REQUIRE(line.find("Test PR") != std::string::npos);
  REQUIRE(line.find("o/r") != std::string::npos);

  mvwinnstr(ui.help_win_, 3, 1, buf.data(), 79);
  std::string help_line(buf.data());
  REQUIRE(help_line.find("o - Open PR") != std::string::npos);
  mvwinnstr(ui.help_win_, 4, 1, buf.data(), 79);
  std::string help_line2(buf.data());
  REQUIRE(help_line2.find("ENTER/d - Details") != std::string::npos);

  int prev_get = raw->get_count;
  ui.handle_key('r');
  REQUIRE(raw->get_count > prev_get);

  ui.handle_key('m');
  REQUIRE(raw->last_method == "PUT");
  REQUIRE(raw->last_url.find("/repos/o/r/pulls/1/merge") != std::string::npos);
  REQUIRE(!ui.logs().empty());
  REQUIRE(ui.logs().back().find("Merged PR #1") != std::string::npos);

  ui.cleanup();
}
