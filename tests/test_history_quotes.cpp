#include "history.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace agpm;

TEST_CASE("test history quotes") {
  auto parse_csv = [](const std::string &text) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    std::string field;
    bool in_quotes = false;
    for (size_t i = 0; i < text.size(); ++i) {
      char c = text[i];
      if (in_quotes) {
        if (c == '"') {
          if (i + 1 < text.size() && text[i + 1] == '"') {
            field += '"';
            ++i;
          } else {
            in_quotes = false;
          }
        } else {
          field += c;
        }
      } else {
        if (c == '"') {
          in_quotes = true;
        } else if (c == ',') {
          row.push_back(field);
          field.clear();
        } else if (c == '\n') {
          row.push_back(field);
          rows.push_back(row);
          row.clear();
          field.clear();
        } else if (c == '\r') {
          // ignore
        } else {
          field += c;
        }
      }
    }
    if (!field.empty() || !row.empty()) {
      row.push_back(field);
      rows.push_back(row);
    }
    return rows;
  };

  PullRequestHistory hist("test_history_quotes.db");
  hist.insert(1, "Comma, Title", true);
  hist.insert(2, "Quote \"Title\"", false);
  hist.insert(3, "Line1\nLine2", true);
  hist.export_csv("quotes.csv");

  std::ifstream csv("quotes.csv");
  std::string content((std::istreambuf_iterator<char>(csv)),
                      std::istreambuf_iterator<char>());
  csv.close();

  auto rows = parse_csv(content);
  REQUIRE(rows.size() == 4);
  REQUIRE(rows[0][0] == "number");
  REQUIRE(rows[0][1] == "title");
  REQUIRE(rows[0][2] == "merged");

  REQUIRE(rows[1][0] == "1");
  REQUIRE(rows[1][1] == "Comma, Title");
  REQUIRE(rows[1][2] == "1");

  REQUIRE(rows[2][0] == "2");
  REQUIRE(rows[2][1] == "Quote \"Title\"");
  REQUIRE(rows[2][2] == "0");

  REQUIRE(rows[3][0] == "3");
  REQUIRE(rows[3][1] == "Line1\nLine2");
  REQUIRE(rows[3][2] == "1");

  std::remove("test_history_quotes.db");
  std::remove("quotes.csv");
}
