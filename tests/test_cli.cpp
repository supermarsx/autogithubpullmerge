#include "cli.hpp"
#include <cassert>

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
  return 0;
}
