#include "history.hpp"
#include <fstream>
#include <string_view>

#if defined(__CPPCHECK__)
// Provide minimal stubs for static analysis to avoid header resolution issues.
struct sqlite3;
extern "C" {
int sqlite3_open(const char *, sqlite3 **);
int sqlite3_close(sqlite3 *);
int sqlite3_exec(sqlite3 *, const char *, int (*)(void *, int, char **, char **), void *, char **);
int sqlite3_prepare_v2(sqlite3 *, const char *, int, void **, const char **);
int sqlite3_bind_int(void *, int, int);
int sqlite3_bind_text(void *, int, const char *, int, void (*)(void *));
int sqlite3_step(void *);
void sqlite3_finalize(void *);
const unsigned char *sqlite3_column_text(void *, int);
int sqlite3_column_int(void *, int);
void sqlite3_free(void *);
}
#ifndef SQLITE_OK
#define SQLITE_OK 0
#endif
#ifndef SQLITE_DONE
#define SQLITE_DONE 101
#endif
#ifndef SQLITE_TRANSIENT
#define SQLITE_TRANSIENT ((void (*)(void *))-1)
#endif
#elif __has_include(<sqlite3.h>)
#include <sqlite3.h>
#else
// As a last resort, forward-declare to allow analysis to proceed.
struct sqlite3;
#endif

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

void PullRequestHistory::update_merged(int number) {
  sqlite3_stmt *stmt = nullptr;
  const char *sql = "UPDATE pull_requests SET merged=1 WHERE number=?";
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare update");
  }
  sqlite3_bind_int(stmt, 1, number);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute update");
  }
  sqlite3_finalize(stmt);
}

void PullRequestHistory::export_csv(const std::string &path) {
  std::ofstream out(path);
  if (!out) {
    throw std::runtime_error("Failed to open CSV file");
  }

  auto escape_csv_field = [](std::string_view field) {
    bool needs_wrap = field.find(',') != std::string_view::npos ||
                      field.find('"') != std::string_view::npos ||
                      field.find('\n') != std::string_view::npos ||
                      field.find('\r') != std::string_view::npos;
    std::string escaped;
    escaped.reserve(field.size());
    for (char c : field) {
      if (c == '"') {
        escaped += "\"\"";
      } else {
        escaped += c;
      }
    }
    if (needs_wrap) {
      return std::string("\"") + escaped + "\"";
    }
    return escaped;
  };

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
    out << escape_csv_field(std::to_string(number)) << ','
        << escape_csv_field(title ? reinterpret_cast<const char *>(title) : "")
        << ',' << escape_csv_field(std::to_string(merged)) << '\n';
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
