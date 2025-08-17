#include "github_poller.hpp"
#include "tui.hpp"
#include <cassert>
#include <cstdlib>
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

int main() {
#ifdef _WIN32
  _putenv_s("TERM", "xterm");
#else
  setenv("TERM", "xterm", 1);
#endif

  auto mock = std::make_unique<MockHttpClient>();
  mock->get_response = "[{\"number\":1,\"title\":\"Test PR\"}]";
  mock->put_response = "{\"merged\":true}";
  MockHttpClient *raw = mock.get();
  GitHubClient client("token", std::unique_ptr<HttpClient>(mock.release()));
  GitHubPoller poller(client, {{"o", "r"}}, 1000, 60);
  Tui ui(client, poller);
  ui.init();

  ui.update_prs({{1, "Test PR", "o", "r"}});
  ui.draw();
  char buf[80];
  mvwinnstr(stdscr, 1, 1, buf, 79);
  assert(std::string(buf).find("Test PR") != std::string::npos);

  int prev_get = raw->get_count;
  ui.handle_key('r');
  assert(raw->get_count > prev_get);

  ui.handle_key('m');
  assert(raw->last_method == "PUT");
  assert(raw->last_url.find("/repos/o/r/pulls/1/merge") != std::string::npos);
  assert(!ui.logs().empty());
  assert(ui.logs().back().find("Merged PR #1") != std::string::npos);

  ui.cleanup();
  return 0;
}
