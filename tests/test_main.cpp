#include "app.hpp"
#include "github_client.hpp"
#include <atomic>
#include <cassert>
#include <thread>
#include <vector>

#if __has_include(<curses.h>)
#include <curses.h>
#elif __has_include(<ncurses.h>)
#include <ncurses.h>
#elif __has_include(<ncurses/curses.h>)
#include <ncurses/curses.h>
#endif

class CountHttpClient : public agpm::HttpClient {
public:
  std::atomic<int> &counter;
  explicit CountHttpClient(std::atomic<int> &c) : counter(c) {}
  std::string get(const std::string &url,
                  const std::vector<std::string> &headers) override {
    (void)url;
    (void)headers;
    ++counter;
    return "[]";
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
    return {};
  }
};

int main() {
  agpm::App app;
  std::atomic<int> count{0};
  app.set_http_client(std::make_unique<CountHttpClient>(count));

  std::vector<char *> args;
  char prog[] = "tests";
  char include_flag[] = "--include";
  char repo_arg[] = "o/r";
  char poll_flag[] = "--poll-interval";
  char poll_val[] = "0";
  char api_flag[] = "--api-key";
  char token[] = "tok";
  args.push_back(prog);
  args.push_back(include_flag);
  args.push_back(repo_arg);
  args.push_back(poll_flag);
  args.push_back(poll_val);
  args.push_back(api_flag);
  args.push_back(token);

#ifdef _WIN32
  _putenv_s("TERM", "xterm");
#else
  setenv("TERM", "xterm", 1);
#endif

  std::thread key_thread([] {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ungetch('q');
  });

  assert(app.run(static_cast<int>(args.size()), args.data()) == 0);
  key_thread.join();
  assert(count > 0);
  return 0;
}
