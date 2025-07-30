#ifndef AUTOGITHUBPULLMERGE_APP_HPP
#define AUTOGITHUBPULLMERGE_APP_HPP

#include "cli.hpp"

namespace agpm {

class App {
public:
  int run(int argc, char **argv);
  const CliOptions &options() const { return options_; }

private:
  CliOptions options_;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_APP_HPP
