#include "cli.hpp"
#include <cassert>
#include <fstream>
#include <sstream>

int main() {
  // Load tokens from YAML file
  {
    std::ofstream f("tokens.yaml");
    f << "tokens:\n  - a\n  - b\n";
    f.close();
  }
  char prog[] = "prog";
  char flag[] = "--api-key-file";
  char file[] = "tokens.yaml";
  char *argv1[] = {prog, flag, file};
  agpm::CliOptions opts = agpm::parse_cli(3, argv1);
  assert(opts.api_keys.size() == 2);
  assert(opts.api_keys[0] == "a");
  assert(opts.api_keys[1] == "b");

  // Multiple --api-key options
  char api_flag[] = "--api-key";
  char k1[] = "c";
  char k2[] = "d";
  char *argv2[] = {prog, api_flag, k1, api_flag, k2};
  agpm::CliOptions opts2 = agpm::parse_cli(5, argv2);
  assert(opts2.api_keys.size() == 2);
  assert(opts2.api_keys[0] == "c");
  assert(opts2.api_keys[1] == "d");

  // Tokens from stdin
  {
    std::istringstream input("e\nf\n\n");
    auto *cinbuf = std::cin.rdbuf();
    std::cin.rdbuf(input.rdbuf());
    char *argv3[] = {prog, const_cast<char *>("--api-key-from-stream")};
    agpm::CliOptions opts3 = agpm::parse_cli(2, argv3);
    std::cin.rdbuf(cinbuf);
    assert(opts3.api_keys.size() == 2);
    assert(opts3.api_keys[0] == "e");
    assert(opts3.api_keys[1] == "f");
  }

  return 0;
}
