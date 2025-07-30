#include "history.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>

using namespace agpm;

int main() {
  PullRequestHistory hist("test_history.db");
  hist.insert(1, "Test PR", true);
  hist.export_csv("out.csv");
  hist.export_json("out.json");

  std::ifstream csv("out.csv");
  std::string line;
  std::getline(csv, line);
  assert(line == "number,title,merged");
  std::getline(csv, line);
  assert(line.find("1") != std::string::npos);
  assert(line.find("Test PR") != std::string::npos);
  csv.close();

  std::ifstream js("out.json");
  nlohmann::json j;
  js >> j;
  assert(j.is_array());
  assert(j.size() == 1);
  assert(j[0]["number"] == 1);
  assert(j[0]["title"] == "Test PR");
  assert(j[0]["merged"] == true);

  std::remove("test_history.db");
  std::remove("out.csv");
  std::remove("out.json");
  return 0;
}
