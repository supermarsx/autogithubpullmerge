#include "log.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <spdlog/spdlog.h>

int main() {
  const char *path = "test.log";
  std::remove(path);
  agpm::init_logger(spdlog::level::info, "", path);
  spdlog::debug("debug message");
  spdlog::info("info message");
  spdlog::shutdown();
  std::ifstream f(path);
  assert(f.good());
  std::string content((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
  assert(content.find("info message") != std::string::npos);
  assert(content.find("debug message") == std::string::npos);
  std::remove(path);
  return 0;
}
