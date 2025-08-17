#include "app.hpp"
#include "cli.hpp"
#include "config.hpp"
#include "github_client.hpp"
#include "github_poller.hpp"
#include "history.hpp"
#include "log.hpp"
#include "tui.hpp"
#include <cstdlib>
#include <spdlog/spdlog.h>

namespace agpm {

int App::run(int argc, char **argv) {
  options_ = parse_cli(argc, argv);
  if (!options_.config_file.empty()) {
    config_ = Config::from_file(options_.config_file);
  }
  include_repos_ = !options_.include_repos.empty() ? options_.include_repos
                                                   : config_.include_repos();
  exclude_repos_ = !options_.exclude_repos.empty() ? options_.exclude_repos
                                                   : config_.exclude_repos();
  std::string level_str = options_.verbose ? "debug" : "info";
  if (options_.log_level != "info") {
    level_str = options_.log_level;
  } else if (config_.log_level() != "info") {
    level_str = config_.log_level();
  }
  spdlog::level::level_enum lvl = spdlog::level::info;
  try {
    lvl = spdlog::level::from_str(level_str);
  } catch (const spdlog::spdlog_ex &) {
    // keep default
  }
  std::string pattern = config_.log_pattern();
  std::string log_file = config_.log_file();
  if (!options_.log_file.empty()) {
    log_file = options_.log_file;
  }
  init_logger(lvl, pattern, log_file);
  if (options_.verbose) {
    spdlog::debug("Verbose mode enabled");
  }
  spdlog::info("Running agpm app");

  std::string token;
  if (!options_.api_keys.empty()) {
    token = options_.api_keys.front();
  } else if (!config_.api_keys().empty()) {
    token = config_.api_keys().front();
  }

  int max_rate = options_.max_request_rate != 60 ? options_.max_request_rate
                                                 : config_.max_request_rate();
  if (max_rate <= 0)
    max_rate = 60;
  int delay_ms = max_rate > 0 ? 60000 / max_rate : 0;

  std::unique_ptr<HttpClient> http = std::move(http_client_);
  if (!http) {
    http = std::make_unique<CurlHttpClient>();
  }
  GitHubClient client(token, std::move(http), include_repos_, exclude_repos_,
                      delay_ms);

  int interval = options_.poll_interval != 0 ? options_.poll_interval
                                             : config_.poll_interval();
  int interval_ms = interval * 1000;

  bool only_poll_prs = options_.only_poll_prs || config_.only_poll_prs();
  bool only_poll_stray = options_.only_poll_stray || config_.only_poll_stray();
  bool reject_dirty = options_.reject_dirty || config_.reject_dirty();
  std::string purge_prefix = !options_.purge_prefix.empty()
                                 ? options_.purge_prefix
                                 : config_.purge_prefix();
  bool auto_merge = options_.auto_merge || config_.auto_merge();
  bool purge_only = options_.purge_only || config_.purge_only();

  std::vector<std::pair<std::string, std::string>> repos;
  for (const auto &r : include_repos_) {
    auto pos = r.find('/');
    if (pos != std::string::npos) {
      repos.emplace_back(r.substr(0, pos), r.substr(pos + 1));
    }
  }

  std::string history_db =
      !options_.history_db.empty() ? options_.history_db : config_.history_db();
  PullRequestHistory history(history_db);

  GitHubPoller poller(client, repos, interval_ms, max_rate, only_poll_prs,
                      only_poll_stray, reject_dirty, purge_prefix, auto_merge,
                      purge_only);
  Tui ui(client, poller);

  poller.start();
  try {
    ui.init();
    ui.run();
  } catch (...) {
    poller.stop();
    ui.cleanup();
    throw;
  }
  poller.stop();
  ui.cleanup();

  (void)history; // history saved on destruction

  return 0;
}

} // namespace agpm
