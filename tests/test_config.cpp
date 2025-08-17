#include "config.hpp"
#include <cassert>
#include <chrono>
#include <fstream>

int main() {
  // YAML config with extended options
  {
    std::ofstream f("cfg.yaml");
    f << "verbose: true\n";
    f << "poll_interval: 3\n";
    f << "max_request_rate: 10\n";
    f << "log_level: debug\n";
    f << "include_repos:\n  - repoA\n  - repoB\n";
    f << "exclude_repos:\n  - repoC\n";
    f << "api_keys:\n  - a\n  - b\n";
    f << "include_merged: true\n";
    f << "history_db: hist.db\n";
    f << "only_poll_prs: true\n";
    f << "reject_dirty: true\n";
    f << "auto_merge: true\n";
    f << "purge_prefix: tmp/\n";
    f << "pr_limit: 25\n";
    f << "pr_since: 2h\n";
    f << "sort: reverse\n";
    f.close();
  }
  agpm::Config yaml_cfg = agpm::Config::from_file("cfg.yaml");
  assert(yaml_cfg.verbose());
  assert(yaml_cfg.poll_interval() == 3);
  assert(yaml_cfg.max_request_rate() == 10);
  assert(yaml_cfg.log_level() == "debug");
  assert(yaml_cfg.include_repos().size() == 2);
  assert(yaml_cfg.include_repos()[0] == "repoA");
  assert(yaml_cfg.exclude_repos().size() == 1);
  assert(yaml_cfg.exclude_repos()[0] == "repoC");
  assert(yaml_cfg.api_keys().size() == 2);
  assert(yaml_cfg.include_merged());
  assert(yaml_cfg.history_db() == "hist.db");
  assert(yaml_cfg.only_poll_prs());
  assert(yaml_cfg.reject_dirty());
  assert(yaml_cfg.auto_merge());
  assert(yaml_cfg.purge_prefix() == "tmp/");
  assert(yaml_cfg.pr_limit() == 25);
  assert(yaml_cfg.pr_since() == std::chrono::hours(2));
  assert(yaml_cfg.sort_mode() == "reverse");

  // JSON config with extended options
  {
    std::ofstream f("cfg.json");
    f << "{";
    f << "\"verbose\": false,";
    f << "\"poll_interval\":2,";
    f << "\"max_request_rate\":5,";
    f << "\"log_level\":\"warn\",";
    f << "\"include_repos\":[\"x\"],";
    f << "\"exclude_repos\":[\"y\",\"z\"],";
    f << "\"api_keys\":[\"k1\"],";
    f << "\"history_db\":\"db.sqlite\",";
    f << "\"only_poll_stray\":true,";
    f << "\"purge_prefix\":\"test/\",";
    f << "\"pr_limit\":30,";
    f << "\"pr_since\":\"15m\",";
    f << "\"sort\":\"alphanum\"";
    f << "}";
    f.close();
  }
  agpm::Config json_cfg = agpm::Config::from_file("cfg.json");
  assert(!json_cfg.verbose());
  assert(json_cfg.poll_interval() == 2);
  assert(json_cfg.max_request_rate() == 5);
  assert(json_cfg.log_level() == "warn");
  assert(json_cfg.include_repos().size() == 1);
  assert(json_cfg.include_repos()[0] == "x");
  assert(json_cfg.exclude_repos().size() == 2);
  assert(json_cfg.exclude_repos()[1] == "z");
  assert(json_cfg.api_keys().size() == 1);
  assert(json_cfg.history_db() == "db.sqlite");
  assert(json_cfg.only_poll_stray());
  assert(json_cfg.purge_prefix() == "test/");
  assert(json_cfg.pr_limit() == 30);
  assert(json_cfg.pr_since() == std::chrono::minutes(15));
  assert(json_cfg.sort_mode() == "alphanum");

  return 0;
}
