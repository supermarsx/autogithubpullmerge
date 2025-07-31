#include "cli.hpp"
#include <cassert>

int main() {
  char prog[] = "prog";
  char verbose[] = "--verbose";
  char *argv1[] = {prog, verbose};
  agpm::CliOptions opts = agpm::parse_cli(2, argv1);
  assert(opts.verbose);

  char loglevel[] = "--log-level";
  char debug[] = "debug";
  char *argv2a[] = {prog, loglevel, debug};
  agpm::CliOptions opts_level = agpm::parse_cli(3, argv2a);
  assert(opts_level.log_level == "debug");

  char *argv2[] = {prog};
  agpm::CliOptions opts2 = agpm::parse_cli(1, argv2);
  assert(!opts2.verbose);
  assert(opts2.log_level == "info");
  return 0;
}
