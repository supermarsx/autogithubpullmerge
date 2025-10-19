#include "cli.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <system_error>
#include <unordered_set>

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
  std::filesystem::remove("tokens.yaml");

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

  // Auto-detect token files in current directory
  std::filesystem::path autodetect_path =
      std::filesystem::current_path() / "autodetect.tokens.toml";
  {
    std::ofstream f(autodetect_path);
    f << "tokens=[\"g\",\"h\"]\n";
  }
  char auto_flag[] = "--auto-detect-token-files";
  char *argv_auto[] = {prog, auto_flag};
  agpm::CliOptions auto_opts = agpm::parse_cli(2, argv_auto);
  REQUIRE(auto_opts.api_keys.size() >= 2);
  REQUIRE_FALSE(auto_opts.auto_detected_api_key_files.empty());
  std::error_code ec;
  auto expected = std::filesystem::weakly_canonical(autodetect_path, ec);
  if (ec) {
    expected = std::filesystem::absolute(autodetect_path);
  }
  std::unordered_set<std::string> detected(auto_opts.auto_detected_api_key_files.begin(),
                                           auto_opts.auto_detected_api_key_files.end());
  REQUIRE(detected.count(expected.string()) == 1);
  std::filesystem::remove(autodetect_path);

  // Ensure explicit files are not double-counted when auto-detect is enabled
  std::filesystem::path duplicate_path =
      std::filesystem::current_path() / "duplicate_tokens.yaml";
  {
    std::ofstream f(duplicate_path);
    f << "tokens:\n  - i\n  - j\n";
  }
  char dup_file[] = "duplicate_tokens.yaml";
  char *argv_dup[] = {prog, flag, dup_file, auto_flag};
  agpm::CliOptions dup_opts = agpm::parse_cli(4, argv_dup);
  REQUIRE(dup_opts.api_keys.size() == 2);
  REQUIRE(dup_opts.auto_detected_api_key_files.empty());
  std::filesystem::remove(duplicate_path);
}
