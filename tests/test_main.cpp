#include "app.hpp"
#include "config.hpp"
#include "github_client.hpp"
#include "github_poller.hpp"
#include "history.hpp"
#include "tui.hpp"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

TEST_CASE("test main") {
  agpm::App app;
  std::vector<char *> args;
  char prog[] = "tests";
  char verbose[] = "--verbose";
  char include_flag[] = "--include";
  char repo_arg[] = "o/r";
  args.push_back(prog);
  args.push_back(verbose);
  args.push_back(include_flag);
  args.push_back(repo_arg);
  REQUIRE(app.run(static_cast<int>(args.size()), args.data()) == 0);
  REQUIRE(app.options().verbose);

#ifdef _WIN32
  _putenv_s("TERM", "xterm");
#else
  setenv("TERM", "xterm", 1);
#endif

  agpm::PullRequestHistory hist(app.options().history_db);
  (void)hist;

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

  std::atomic<int> count{0};
  auto http = std::make_unique<CountHttpClient>(count);
  agpm::GitHubClient client("tok",
                            std::unique_ptr<agpm::HttpClient>(http.release()),
                            app.include_repos(), app.exclude_repos(), 0);
  std::vector<std::pair<std::string, std::string>> repos;
  for (const auto &r : app.include_repos()) {
    auto pos = r.find('/');
    if (pos != std::string::npos) {
      repos.emplace_back(r.substr(0, pos), r.substr(pos + 1));
    }
  }
  agpm::GitHubPoller poller(client, repos, 10, 60);
  poller.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  poller.stop();
  REQUIRE(count > 0);

  agpm::Tui ui(client, poller);
  ui.init();
  ui.cleanup();

  agpm::App app_cfg;
  std::ofstream cfg("run_config.yaml");
  cfg << "verbose: true\n";
  cfg.close();
  std::vector<char *> args_cfg;
  char prog2[] = "tests";
  char config_flag[] = "--config";
  char cfg_file[] = "run_config.yaml";
  args_cfg.push_back(prog2);
  args_cfg.push_back(config_flag);
  args_cfg.push_back(cfg_file);
  REQUIRE(app_cfg.run(static_cast<int>(args_cfg.size()), args_cfg.data()) == 0);
  REQUIRE(app_cfg.options().config_file == "run_config.yaml");
  REQUIRE(app_cfg.config().verbose());

  agpm::App log_app;
  std::vector<char *> args_log;
  char prog3[] = "tests";
  char log_flag[] = "--log-level";
  char warn_lvl[] = "warn";
  args_log.push_back(prog3);
  args_log.push_back(log_flag);
  args_log.push_back(warn_lvl);
  REQUIRE(log_app.run(static_cast<int>(args_log.size()), args_log.data()) == 0);
  REQUIRE(log_app.options().log_level == "warn");

  agpm::App hist_app;
  std::vector<char *> hist_args;
  char prog4[] = "tests";
  char db_flag[] = "--history-db";
  char hist_file[] = "hist.db";
  hist_args.push_back(prog4);
  hist_args.push_back(db_flag);
  hist_args.push_back(hist_file);
  REQUIRE(hist_app.run(static_cast<int>(hist_args.size()), hist_args.data()) ==
          0);
  REQUIRE(hist_app.options().history_db == "hist.db");

  {
    std::ofstream yaml("test_config.yaml");
    yaml << "verbose: true\n";
    yaml.close();
    agpm::Config cfg = agpm::Config::from_file("test_config.yaml");
    REQUIRE(cfg.verbose());
  }

  {
    std::ofstream json("test_config.json");
    json << "{\"verbose\": true}";
    json.close();
    agpm::Config cfg = agpm::Config::from_file("test_config.json");
    REQUIRE(cfg.verbose());
  }

  {
    agpm::App bad_app;
    char prog_err[] = "tests";
    char unknown[] = "--unknown";
    char *args_err[] = {prog_err, unknown};
    REQUIRE(bad_app.run(2, args_err) != 0);
  }

  {
    agpm::App cancel_app;
    std::istringstream input("n\n");
    auto *cinbuf = std::cin.rdbuf();
    std::cin.rdbuf(input.rdbuf());
    char prog_err2[] = "tests";
    char auto_merge_flag2[] = "--auto-merge";
    char *cancel_args[] = {prog_err2, auto_merge_flag2};
    REQUIRE(cancel_app.run(2, cancel_args) != 0);
    std::cin.rdbuf(cinbuf);
  }
}
