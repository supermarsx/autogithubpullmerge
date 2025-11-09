/**
 * @file history.cpp
 * @brief Implements persistent storage and export of pull request history using
 * SQLite.
 *
 * This file defines the PullRequestHistory class, which manages a local SQLite
 * database for tracking pull request metadata (number, title, merged status)
 * and provides export functionality to CSV and JSON formats.
 */
#include "history.hpp"
#include "log.hpp"
#include <fstream>
#include <string_view>

#if defined(__CPPCHECK__)
// Provide minimal stubs for static analysis to avoid header resolution issues.
struct sqlite3;
extern "C" {
int sqlite3_open(const char *, sqlite3 **);
int sqlite3_close(sqlite3 *);
int sqlite3_exec(sqlite3 *, const char *,
                 int (*)(void *, int, char **, char **), void *, char **);
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
#define SQLITE_TRANSIENT ((void (*)(void *)) - 1)
#endif
#elif __has_include(<sqlite3.h>)
#include <sqlite3.h>
#else
// As a last resort, forward-declare to allow analysis to proceed.
struct sqlite3;
#endif

#include <memory>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace agpm {

namespace {
std::shared_ptr<spdlog::logger> history_log() {
  static auto logger = [] {
    ensure_default_logger();
    return category_logger("history");
  }();
  return logger;
}
} // namespace

/**
 * Initialize the pull request history database connection.
 *
 * @param db_path Path to the SQLite database file to open or create.
 * @throws std::runtime_error When the database cannot be opened or the schema
 *         initialization fails.
 */
PullRequestHistory::PullRequestHistory(const std::string &db_path) {
  history_log()->debug("History: opening DB {}", db_path);
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
  history_log()->debug("History: DB initialized");
}

/**
 * Close the database connection on destruction.
 */
PullRequestHistory::~PullRequestHistory() {
  history_log()->debug("History: closing DB");
  if (db_) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

/**
 * Record a pull request entry.
 *
 * @param number Numeric pull request identifier.
 * @param title Pull request title string.
 * @param merged Whether the pull request has been merged.
 * @throws std::runtime_error When the insert statement fails.
 */
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

/**
 * Mark a pull request as merged.
 *
 * @param number Numeric pull request identifier to update.
 * @throws std::runtime_error When the update statement fails.
 */
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

/**
 * Export history entries to a CSV file.
 *
 * @param path Destination file path for the CSV export.
 * @throws std::runtime_error On database query errors or I/O failures.
 */
void PullRequestHistory::export_csv(const std::string &path) {
  history_log()->debug("History: export_csv -> {}", path);
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
  history_log()->debug("History: export_csv done");
}

/**
 * Export history entries to a JSON file.
 *
 * @param path Destination file path for the JSON export.
 * @throws std::runtime_error On database query errors or I/O failures.
 */
void PullRequestHistory::export_json(const std::string &path) {
  history_log()->debug("History: export_json -> {}", path);
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
  history_log()->debug("History: export_json done");
}

} // namespace agpm
