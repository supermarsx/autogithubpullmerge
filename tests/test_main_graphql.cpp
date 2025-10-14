#include "app.hpp"
#include "config.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <vector>

TEST_CASE("use_graphql honored in main", "[cli]") {
  SECTION("enabled via CLI") {
    agpm::App app;
    std::vector<char *> args;
    char prog[] = "tests";
    char flag[] = "--use-graphql";
    args.push_back(prog);
    args.push_back(flag);
    REQUIRE(app.run(static_cast<int>(args.size()), args.data()) == 0);
    const auto &opts = app.options();
    const auto &cfg = app.config();
    bool use_graphql = opts.use_graphql || cfg.use_graphql();
    REQUIRE(use_graphql);
  }

  SECTION("enabled via config file") {
    agpm::App app;
    std::filesystem::path cfg_file_path =
        std::filesystem::temp_directory_path() / "agpm_graphql_config.yaml";
    {
      std::ofstream f(cfg_file_path.string());
      f << "use_graphql: true\n";
    }
    std::vector<char *> args;
    char prog[] = "tests";
    char cflag[] = "--config";
    std::string file_str = cfg_file_path.string();
    char *file = const_cast<char *>(file_str.c_str());
    args.push_back(prog);
    args.push_back(cflag);
    args.push_back(file);
    REQUIRE(app.run(static_cast<int>(args.size()), args.data()) == 0);
    const auto &opts = app.options();
    const auto &cfg = app.config();
    REQUIRE(cfg.use_graphql());
    bool use_graphql = opts.use_graphql || cfg.use_graphql();
    REQUIRE(use_graphql);
  }
}
