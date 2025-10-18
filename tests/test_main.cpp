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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <unordered_set>
#include <vector>

#if defined(_WIN32)
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

TEST_CASE("main cli run", "[cli]") {
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
}

TEST_CASE("main poller runs", "[cli]") {
  // Ensure poller starts and triggers at least one request
  agpm::App app;
  std::vector<char *> args;
  char prog[] = "tests";
  char include_flag[] = "--include";
  char repo_arg[] = "o/r";
  args.push_back(prog);
  args.push_back(include_flag);
  args.push_back(repo_arg);
  REQUIRE(app.run(static_cast<int>(args.size()), args.data()) == 0);

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
  std::unordered_set<std::string> include_set(app.include_repos().begin(),
                                              app.include_repos().end());
  std::unordered_set<std::string> exclude_set(app.exclude_repos().begin(),
                                              app.exclude_repos().end());
  agpm::GitHubClient client({"tok"},
                            std::unique_ptr<agpm::HttpClient>(http.release()),
                            include_set, exclude_set, 0);
  std::vector<std::pair<std::string, std::string>> repos;
  for (const auto &r : app.include_repos()) {
    auto pos = r.find('/');
    if (pos != std::string::npos) {
      repos.emplace_back(r.substr(0, pos), r.substr(pos + 1));
    }
  }
  agpm::GitHubPoller poller(client, repos, 10, 60);
  poller.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  poller.stop();
  REQUIRE(count > 0);
}

TEST_CASE("main config load", "[cli]") {
  agpm::App app_cfg;
  std::filesystem::path cfg_path =
      std::filesystem::temp_directory_path() / "agpm_run_config.yaml";
  {
    std::ofstream cfg(cfg_path.string());
    cfg << "verbose: true\n";
  }
  std::vector<char *> args_cfg;
  char prog2[] = "tests";
  char config_flag[] = "--config";
  std::string cfg_file_str = cfg_path.string();
  char *cfg_file = const_cast<char *>(cfg_file_str.c_str());
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
    std::filesystem::path yaml_path =
        std::filesystem::temp_directory_path() / "agpm_test_config.yaml";
    std::ofstream yaml(yaml_path.string());
    yaml << "verbose: true\n";
    yaml.close();
    agpm::Config cfg = agpm::Config::from_file(yaml_path.string());
    REQUIRE(cfg.verbose());
  }

  {
    std::filesystem::path json_path =
        std::filesystem::temp_directory_path() / "agpm_test_config.json";
    std::ofstream json(json_path.string());
    json << "{\"verbose\": true}";
    json.close();
    agpm::Config cfg = agpm::Config::from_file(json_path.string());
    REQUIRE(cfg.verbose());
  }
}

TEST_CASE("main invalid option", "[cli]") {
  agpm::App bad_app;
  char prog_err[] = "tests";
  char unknown[] = "--unknown";
  char *args_err[] = {prog_err, unknown};
  REQUIRE(bad_app.run(2, args_err) != 0);
}

TEST_CASE("main auto-merge cancel", "[cli]") {
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

TEST_CASE("app open pat page exits after launch", "[cli]") {
#ifdef _WIN32
  _putenv_s("AGPM_TEST_SKIP_BROWSER", "1");
#else
  setenv("AGPM_TEST_SKIP_BROWSER", "1", 1);
#endif
  agpm::App app;
  char prog[] = "tests";
  char open_pat_flag[] = "--open-pat-page";
  char *argv[] = {prog, open_pat_flag};
  REQUIRE(app.run(2, argv) == 0);
  REQUIRE(app.should_exit());
#ifdef _WIN32
  _putenv_s("AGPM_TEST_SKIP_BROWSER", "0");
#else
  setenv("AGPM_TEST_SKIP_BROWSER", "0", 1);
#endif
}

TEST_CASE("app saves pat from cli value", "[cli]") {
  agpm::App app;
  std::filesystem::path pat_path =
      std::filesystem::temp_directory_path() / "agpm_test_pat.txt";
  std::filesystem::remove(pat_path);
  char prog[] = "tests";
  char save_pat_flag[] = "--save-pat";
  std::string pat_path_str = pat_path.string();
  char *pat_file = pat_path_str.data();
  char pat_value_flag[] = "--pat-value";
  std::string value_str = "ghp_cli_value";
  char *value_cstr = value_str.data();
  char *argv[] = {prog, save_pat_flag, pat_file, pat_value_flag, value_cstr};
  REQUIRE(app.run(5, argv) == 0);
  REQUIRE(app.should_exit());
  std::ifstream in(pat_path);
  REQUIRE(in.good());
  std::string stored;
  std::getline(in, stored);
  REQUIRE(stored == value_str);
  in.close();
  std::filesystem::remove(pat_path);
}

TEST_CASE("app saves pat via prompt", "[cli]") {
  agpm::App app;
  std::filesystem::path pat_path =
      std::filesystem::temp_directory_path() / "agpm_test_pat_prompt.txt";
  std::filesystem::remove(pat_path);
  char prog[] = "tests";
  char save_pat_flag[] = "--save-pat";
  std::string pat_path_str = pat_path.string();
  char *pat_file = pat_path_str.data();
  char *argv[] = {prog, save_pat_flag, pat_file};
  std::istringstream input("ghp_prompt_value\n");
  auto *cinbuf = std::cin.rdbuf();
  std::cin.rdbuf(input.rdbuf());
  REQUIRE(app.run(3, argv) == 0);
  std::cin.rdbuf(cinbuf);
  REQUIRE(app.should_exit());
  std::ifstream in(pat_path);
  REQUIRE(in.good());
  std::string stored;
  std::getline(in, stored);
  REQUIRE(stored == "ghp_prompt_value");
  in.close();
  std::filesystem::remove(pat_path);
}

TEST_CASE("config overrides populate cli options", "[cli]") {
  std::filesystem::path cfg_path =
      std::filesystem::temp_directory_path() / "agpm_config_merge.yaml";
  {
    std::ofstream cfg(cfg_path.string());
    cfg << "dry_run: true\n";
    cfg << "assume_yes: true\n";
    cfg << "log_limit: 321\n";
    cfg << "log_rotate: 4\n";
    cfg << "log_compress: true\n";
    cfg << "export_csv: config-export.csv\n";
    cfg << "export_json: config-export.json\n";
    cfg << "delete_stray: true\n";
    cfg << "single_open_prs_repo: foo/bar\n";
    cfg << "single_branches_repo: foo/bar\n";
  }
  agpm::App app;
  std::vector<char *> args;
  char prog[] = "tests";
  char config_flag[] = "--config";
  std::string cfg_path_str = cfg_path.string();
  char *cfg_cstr = cfg_path_str.data();
  args.push_back(prog);
  args.push_back(config_flag);
  args.push_back(cfg_cstr);
  REQUIRE(app.run(static_cast<int>(args.size()), args.data()) == 0);
  REQUIRE(app.options().dry_run);
  REQUIRE(app.options().assume_yes);
  REQUIRE(app.options().log_limit == 321);
  REQUIRE(app.options().log_rotate == 4);
  REQUIRE(app.options().log_compress);
  REQUIRE(app.options().export_csv == "config-export.csv");
  REQUIRE(app.options().export_json == "config-export.json");
  REQUIRE(app.options().delete_stray);
  REQUIRE(app.options().single_open_prs_repo == "foo/bar");
  REQUIRE(app.options().single_branches_repo == "foo/bar");
  std::filesystem::remove(cfg_path);
}
