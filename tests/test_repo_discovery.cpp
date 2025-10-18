#include "repo_discovery.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

TEST_CASE("repo discovery string conversion") {
  REQUIRE(agpm::repo_discovery_mode_from_string("disabled") ==
          agpm::RepoDiscoveryMode::Disabled);
  REQUIRE(agpm::repo_discovery_mode_from_string("DISABLED") ==
          agpm::RepoDiscoveryMode::Disabled);
  REQUIRE(agpm::repo_discovery_mode_from_string("all") ==
          agpm::RepoDiscoveryMode::All);
  REQUIRE(agpm::repo_discovery_mode_from_string("Auto") ==
          agpm::RepoDiscoveryMode::All);
  REQUIRE(agpm::repo_discovery_mode_from_string("filesystem") ==
          agpm::RepoDiscoveryMode::Filesystem);

  REQUIRE(agpm::to_string(agpm::RepoDiscoveryMode::Disabled) == "disabled");
  REQUIRE(agpm::to_string(agpm::RepoDiscoveryMode::All) == "all");
  REQUIRE(agpm::to_string(agpm::RepoDiscoveryMode::Filesystem) == "filesystem");

  REQUIRE_FALSE(agpm::repo_discovery_enabled(agpm::RepoDiscoveryMode::Disabled));
  REQUIRE(agpm::repo_discovery_enabled(agpm::RepoDiscoveryMode::All));
  REQUIRE(agpm::repo_discovery_enabled(agpm::RepoDiscoveryMode::Filesystem));
  REQUIRE(agpm::repo_discovery_uses_tokens(agpm::RepoDiscoveryMode::All));
  REQUIRE_FALSE(agpm::repo_discovery_uses_tokens(agpm::RepoDiscoveryMode::Filesystem));
  REQUIRE(agpm::repo_discovery_uses_filesystem(agpm::RepoDiscoveryMode::Filesystem));
  REQUIRE_FALSE(agpm::repo_discovery_uses_filesystem(agpm::RepoDiscoveryMode::All));

  REQUIRE_THROWS_AS(agpm::repo_discovery_mode_from_string("unknown"),
                    std::invalid_argument);
}

TEST_CASE("filesystem repo discovery parses git remotes") {
  namespace fs = std::filesystem;
  auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  fs::path root = fs::temp_directory_path() /
                  fs::path("agpm_repo_discovery_" + std::to_string(stamp));
  fs::create_directories(root);

  struct Cleanup {
    fs::path path;
    ~Cleanup() {
      std::error_code ec;
      fs::remove_all(path, ec);
    }
  } cleanup{root};

  fs::path repo_dir = root / "sample";
  fs::create_directories(repo_dir / ".git");
  {
    std::ofstream cfg(repo_dir / ".git" / "config");
    cfg << "[remote \"origin\"]\n";
    cfg << "    url = https://github.com/example/sample.git\n";
  }

  fs::path repo_dir2 = root / "invalid";
  fs::create_directories(repo_dir2 / ".git");
  {
    std::ofstream cfg(repo_dir2 / ".git" / "config");
    cfg << "[remote \"origin\"]\n";
    cfg << "    url = https://gitlab.com/example/skip.git\n";
  }

  auto repos = agpm::discover_repositories_from_filesystem({root.string()});
  REQUIRE(repos.size() == 1);
  REQUIRE(repos[0].first == "example");
  REQUIRE(repos[0].second == "sample");
}
