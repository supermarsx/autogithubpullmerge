#include "history.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>

using namespace agpm;

int main() {
  PullRequestHistory hist("test_history_quotes.db");
  hist.insert(1, "Comma, Title", true);
  hist.insert(2, "Quote \"Title\"", false);
  hist.export_csv("quotes.csv");

  std::ifstream csv("quotes.csv");
  std::string line;
  std::getline(csv, line);
  assert(line == "number,title,merged");
  std::getline(csv, line);
  assert(line == "1,\"Comma, Title\",1");
  std::getline(csv, line);
  assert(line == "2,\"Quote \"\"Title\"\"\",0");
  csv.close();

  std::remove("test_history_quotes.db");
  std::remove("quotes.csv");
  return 0;
}
