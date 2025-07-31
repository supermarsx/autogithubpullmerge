#include "history.hpp"
#include <fstream>
#include <sqlite3.h>
#include <stdexcept>

namespace agpm {

PullRequestHistory::PullRequestHistory(const std::string &db_path) {
  if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
    throw std::runtime_error("Failed to open database");
  }
  const char *sql = "CREATE TABLE IF NOT EXISTS pull_requests("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "number INTEGER, title TEXT, merged INTEGER);";
  char *err = nullptr;
  if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
    std::string msg = err ? err : "";
    sqlite3_free(err);
    throw std::runtime_error("Failed to create table: " + msg);
  }
}

PullRequestHistory::~PullRequestHistory() {
  if (db_) {
    sqlite3_close(db_);
  }
}

void PullRequestHistory::insert(int number, const std::string &title,
                                bool merged) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql =
      "INSERT INTO pull_requests(number,title,merged) VALUES(?,?,?)";
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare insert");
  }
  sqlite3_bind_int(stmt, 1, number);
  sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 3, merged ? 1 : 0);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute insert");
  }
  sqlite3_finalize(stmt);
}

void PullRequestHistory::export_csv(const std::string &path) {
  std::ofstream out(path);
  if (!out) {
    throw std::runtime_error("Failed to open CSV file");
  }
  out << "number,title,merged\n";
  const char *sql = "SELECT number,title,merged FROM pull_requests";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to query database");
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int number = sqlite3_column_int(stmt, 0);
    const unsigned char *title = sqlite3_column_text(stmt, 1);
    int merged = sqlite3_column_int(stmt, 2);
    out << number << ",\""
        << (title ? reinterpret_cast<const char *>(title) : "") << "\","
        << merged << "\n";
  }
  sqlite3_finalize(stmt);
}

void PullRequestHistory::export_json(const std::string &path) {
  nlohmann::json j = nlohmann::json::array();
  const char *sql = "SELECT number,title,merged FROM pull_requests";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to query database");
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int number = sqlite3_column_int(stmt, 0);
    const unsigned char *title = sqlite3_column_text(stmt, 1);
    int merged = sqlite3_column_int(stmt, 2);
    nlohmann::json item;
    item["number"] = number;
    item["title"] = title ? reinterpret_cast<const char *>(title) : "";
    item["merged"] = merged != 0;
    j.push_back(item);
  }
  sqlite3_finalize(stmt);
  std::ofstream out(path);
  if (!out) {
    throw std::runtime_error("Failed to open JSON file");
  }
  out << j.dump(2);
}

} // namespace agpm
