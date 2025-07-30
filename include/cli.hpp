#ifndef AUTOGITHUBPULLMERGE_CLI_HPP
#define AUTOGITHUBPULLMERGE_CLI_HPP

#include <string>

namespace agpm {

struct CliOptions {
  bool verbose = false;
  std::string log_level = "info";
  std::string log_file;
};

CliOptions parse_cli(int argc, char **argv);

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_CLI_HPP
