#include "cli.hpp"
#include <cassert>
#include <chrono>
#include <fstream>

int main() {
  char prog[] = "prog";
  char verbose[] = "--verbose";
  char *argv1[] = {prog, verbose};
  agpm::CliOptions opts = agpm::parse_cli(2, argv1);
  assert(opts.verbose);

  char *argv2[] = {prog};
  agpm::CliOptions opts2 = agpm::parse_cli(1, argv2);
  assert(!opts2.verbose);

  char config_flag[] = "--config";
  char file[] = "cfg.yaml";
  char *argv3[] = {prog, config_flag, file};
  agpm::CliOptions opts3 = agpm::parse_cli(3, argv3);
  assert(opts3.config_file == "cfg.yaml");

  char log_flag[] = "--log-level";
  char debug_lvl[] = "debug";
  char *argv4[] = {prog, log_flag, debug_lvl};
  agpm::CliOptions opts4 = agpm::parse_cli(3, argv4);
  assert(opts4.log_level == "debug");

  char *argv5[] = {prog};
  agpm::CliOptions opts5 = agpm::parse_cli(1, argv5);
  assert(opts5.log_level == "info");

  char include_flag[] = "--include";
  char repo_a[] = "repoA";
  char repo_b[] = "repoB";
  char *argv6[] = {prog, include_flag, repo_a, include_flag, repo_b};
  agpm::CliOptions opts6 = agpm::parse_cli(5, argv6);
  assert(opts6.include_repos.size() == 2);
  assert(opts6.include_repos[0] == "repoA");
  assert(opts6.include_repos[1] == "repoB");

  char exclude_flag[] = "--exclude";
  char repo_c[] = "repoC";
  char *argv7[] = {prog, exclude_flag, repo_c};
  agpm::CliOptions opts7 = agpm::parse_cli(3, argv7);
  assert(opts7.exclude_repos.size() == 1);
  assert(opts7.exclude_repos[0] == "repoC");

  char repo_d[] = "repoD";
  char repo_e[] = "repoE";
  char *argv7b[] = {prog, exclude_flag, repo_d, exclude_flag, repo_e};
  agpm::CliOptions opts7b = agpm::parse_cli(5, argv7b);
  assert(opts7b.exclude_repos.size() == 2);
  assert(opts7b.exclude_repos[0] == "repoD");
  assert(opts7b.exclude_repos[1] == "repoE");

  char api_flag[] = "--api-key";
  char key1[] = "abc";
  char key2[] = "def";
  char *argv8[] = {prog, api_flag, key1, api_flag, key2};
  agpm::CliOptions opts8 = agpm::parse_cli(5, argv8);
  assert(opts8.api_keys.size() == 2);
  assert(opts8.api_keys[0] == "abc");
  assert(opts8.api_keys[1] == "def");

  {
    std::ofstream f("tok.yaml");
    f << "tokens:\n  - a\n  - b\n";
    f.close();
    char file_flag[] = "--api-key-file";
    char path[] = "tok.yaml";
    char *argv9[] = {prog, file_flag, path};
    agpm::CliOptions opts9 = agpm::parse_cli(3, argv9);
    assert(opts9.api_keys.size() == 2);
    assert(opts9.api_keys[0] == "a");
    assert(opts9.api_keys[1] == "b");
  }

  char stream_flag[] = "--api-key-from-stream";
  char *argv10[] = {prog, stream_flag};
  agpm::CliOptions opts10 = agpm::parse_cli(2, argv10);
  assert(opts10.api_key_from_stream);

  char interval_flag[] = "--poll-interval";
  char rate_flag[] = "--max-request-rate";
  char interval_val[] = "5";
  char rate_val[] = "100";
  char *argv11[] = {prog, interval_flag, interval_val, rate_flag, rate_val};
  agpm::CliOptions opts11 = agpm::parse_cli(5, argv11);
  assert(opts11.poll_interval == 5);
  assert(opts11.max_request_rate == 100);

  char db_flag[] = "--history-db";
  char path_db[] = "my.db";
  char *argv12[] = {prog, db_flag, path_db};
  agpm::CliOptions opts12 = agpm::parse_cli(3, argv12);
  assert(opts12.history_db == "my.db");

  char merged_flag[] = "--include-merged";
  char *argv13[] = {prog, merged_flag};
  agpm::CliOptions opts13 = agpm::parse_cli(2, argv13);
  assert(opts13.include_merged);

  char poll_prs_flag[] = "--poll-prs";
  char *argv14[] = {prog, poll_prs_flag};
  agpm::CliOptions opts14 = agpm::parse_cli(2, argv14);
  assert(opts14.poll_prs_only);

  char poll_stray_flag[] = "--poll-stray-branches";
  char *argv15[] = {prog, poll_stray_flag};
  agpm::CliOptions opts15 = agpm::parse_cli(2, argv15);
  assert(opts15.poll_stray_only);

  char limit_flag[] = "--pr-limit";
  char limit_val[] = "25";
  char *argv16[] = {prog, limit_flag, limit_val};
  agpm::CliOptions opts16 = agpm::parse_cli(3, argv16);
  assert(opts16.pr_limit == 25);

  char *argv17[] = {prog};
  agpm::CliOptions opts17 = agpm::parse_cli(1, argv17);
  assert(opts17.pr_limit == 50);

  char since_flag[] = "--pr-since";
  char since_val[] = "2h";
  char *argv18[] = {prog, since_flag, since_val};
  agpm::CliOptions opts18 = agpm::parse_cli(3, argv18);
  assert(opts18.pr_since == std::chrono::hours(2));

  char *argv19[] = {prog};
  agpm::CliOptions opts19 = agpm::parse_cli(1, argv19);
  assert(opts19.pr_since == std::chrono::seconds(0));

  return 0;
}
