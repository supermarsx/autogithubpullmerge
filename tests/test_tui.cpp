#include "github_poller.hpp"
#include "tui.hpp"
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
#include <unordered_map>

using namespace agpm;

namespace {

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

} // namespace

TEST_CASE("test tui", "[tui]") {
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
  mock->get_response = "[{\"number\":1,\"title\":\"Test PR\"}]";
  mock->put_response = "{\"merged\":true}";
  MockHttpClient *raw = mock.get();
  GitHubClient client({"token"}, std::unique_ptr<HttpClient>(mock.release()));
  GitHubPoller poller(client, {{"o", "r"}}, 1000, 60);
  Tui ui(client, poller, 200);
  ui.init();
  if (!ui.initialized()) {
    WARN("Skipping TUI test: no TTY available");
    ui.cleanup();
    return;
  }

  ui.update_prs({{1, "Test PR", false, "o", "r"}});
  ui.draw();
  std::array<char, 80> buf{};
  mvwinnstr(stdscr, 1, 1, buf.data(), 79);
  std::string line(buf.data());
  REQUIRE(line.find("Test PR") != std::string::npos);
  REQUIRE(line.find("o/r") != std::string::npos);

  mvwinnstr(ui.help_win(), 3, 1, buf.data(), 79);
  std::string help_line(buf.data());
  REQUIRE(help_line.find("Open PR") != std::string::npos);
  REQUIRE(help_line.find("o") != std::string::npos);
  mvwinnstr(ui.help_win(), 4, 1, buf.data(), 79);
  std::string help_line2(buf.data());
  REQUIRE(help_line2.find("Toggle Details") != std::string::npos);
  REQUIRE(help_line2.find("Enter") != std::string::npos);

  ui.handle_key('r');
  REQUIRE(raw->last_method == "GET");
  REQUIRE(raw->get_count >= 1);

  const int refresh_count = raw->get_count;
  std::unordered_map<std::string, std::string> overrides{
      {"refresh", "Ctrl+R"},
      {"quit", "Ctrl+Q"}};
  ui.configure_hotkeys(overrides);
  ui.handle_key('r');
  REQUIRE(raw->get_count == refresh_count);
  int ctrl_r = static_cast<int>('R' & 0x1f);
  ui.handle_key(ctrl_r);
  REQUIRE(raw->get_count == refresh_count + 1);
  ui.set_hotkeys_enabled(false);
  ui.handle_key(ctrl_r);
  REQUIRE(raw->get_count == refresh_count + 1);
  ui.set_hotkeys_enabled(true);

  ui.handle_key('m');
  REQUIRE(raw->last_method == "PUT");
  REQUIRE(raw->last_url.find("/repos/o/r/pulls/1/merge") != std::string::npos);
  REQUIRE(!ui.logs().empty());
  REQUIRE(ui.logs().back().find("Merged PR #1") != std::string::npos);

  ui.cleanup();
}
