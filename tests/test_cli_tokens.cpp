#include "cli.hpp"
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <iostream>
#include <sstream>

TEST_CASE("test cli tokens", "[cli]") {
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
  REQUIRE(opts.api_keys.size() == 2);
  REQUIRE(opts.api_keys[0] == "a");
  REQUIRE(opts.api_keys[1] == "b");

  // Multiple --api-key options
  char api_flag[] = "--api-key";
  char k1[] = "c";
  char k2[] = "d";
  char *argv2[] = {prog, api_flag, k1, api_flag, k2};
  agpm::CliOptions opts2 = agpm::parse_cli(5, argv2);
  REQUIRE(opts2.api_keys.size() == 2);
  REQUIRE(opts2.api_keys[0] == "c");
  REQUIRE(opts2.api_keys[1] == "d");

  // Tokens from stdin
  {
    std::istringstream input("e\nf\n\n");
    auto *cinbuf = std::cin.rdbuf();
    std::cin.rdbuf(input.rdbuf());
    char *argv3[] = {prog, const_cast<char *>("--api-key-from-stream")};
    agpm::CliOptions opts3 = agpm::parse_cli(2, argv3);
    std::cin.rdbuf(cinbuf);
    REQUIRE(opts3.api_keys.size() == 2);
    REQUIRE(opts3.api_keys[0] == "e");
    REQUIRE(opts3.api_keys[1] == "f");
  }
}
