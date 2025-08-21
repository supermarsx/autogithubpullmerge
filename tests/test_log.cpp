#include "log.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <fstream>
#include <spdlog/spdlog.h>

TEST_CASE("test log") {
  const char *path = "test.log";
  std::remove(path);
  agpm::init_logger(spdlog::level::info, "", path);
  spdlog::debug("debug message");
  spdlog::info("info message");
  spdlog::shutdown();
  std::ifstream f(path);
  REQUIRE(f.good());
  std::string content((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
  REQUIRE(content.find("info message") != std::string::npos);
  REQUIRE(content.find("debug message") == std::string::npos);
  std::remove(path);
}
