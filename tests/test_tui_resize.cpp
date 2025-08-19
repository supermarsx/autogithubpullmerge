#include "github_poller.hpp"
#define private public
#include "tui.hpp"
#undef private
#include <cassert>
#include <cstdlib>
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

int main() {
#ifdef _WIN32
  _putenv_s("TERM", "xterm");
#else
  setenv("TERM", "xterm", 1);
#endif

  auto mock = std::make_unique<MockHttpClient>();
  GitHubClient client("token", std::move(mock));
  GitHubPoller poller(client, {{"o", "r"}}, 1000, 60);
  Tui ui(client, poller);
  ui.init();

  ui.update_prs({{1, "PR", "o", "r"}});
  ui.draw();
  WINDOW *before = ui.pr_win_;
  int h, w;
  getmaxyx(stdscr, h, w);
  resize_term(h / 2, w / 2);
  ui.draw();
  assert(before != ui.pr_win_);
  ui.cleanup();
  return 0;
}
