#include "history.hpp"
#include <fstream>

#if __has_include(<sqlite3.h>)
#include <sqlite3.h>
#else
#error "sqlite3.h not found"
#endif

#include <stdexcept>

namespace agpm {

/// RAII wrapper for sqlite3_stmt ensuring sqlite3_finalize is called.
class Statement {
public:
  Statement(sqlite3 *db, const char *sql) : stmt_(nullptr) {
    if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
      throw std::runtime_error("Failed to prepare statement");
    }
  }
  ~Statement() {
    if (stmt_)
      sqlite3_finalize(stmt_);
  }
  Statement(const Statement &) = delete;
  Statement &operator=(const Statement &) = delete;
  Statement(Statement &&other) noexcept : stmt_(other.stmt_) {
    other.stmt_ = nullptr;
  }
  Statement &operator=(Statement &&other) noexcept {
    if (this != &other) {
      if (stmt_)
        sqlite3_finalize(stmt_);
      stmt_ = other.stmt_;
      other.stmt_ = nullptr;
    }
    return *this;
  }
  sqlite3_stmt *get() const { return stmt_; }

private:
  sqlite3_stmt *stmt_;
};

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
  const char *sql =
      "INSERT INTO pull_requests(number,title,merged) VALUES(?,?,?)";
  Statement stmt(db_, sql);
  sqlite3_bind_int(stmt.get(), 1, number);
  sqlite3_bind_text(stmt.get(), 2, title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt.get(), 3, merged ? 1 : 0);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
    throw std::runtime_error("Failed to execute insert");
  }
}

void PullRequestHistory::export_csv(const std::string &path) {
  std::ofstream out(path);
  if (!out) {
    throw std::runtime_error("Failed to open CSV file");
  }
  out << "number,title,merged\n";
  const char *sql = "SELECT number,title,merged FROM pull_requests";
  Statement stmt(db_, sql);
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    int number = sqlite3_column_int(stmt.get(), 0);
    const unsigned char *title = sqlite3_column_text(stmt.get(), 1);
    int merged = sqlite3_column_int(stmt.get(), 2);
    out << number << ",\""
        << (title ? reinterpret_cast<const char *>(title) : "") << "\","
        << merged << "\n";
  }
}

void PullRequestHistory::export_json(const std::string &path) {
  nlohmann::json j = nlohmann::json::array();
  const char *sql = "SELECT number,title,merged FROM pull_requests";
  Statement stmt(db_, sql);
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    int number = sqlite3_column_int(stmt.get(), 0);
    const unsigned char *title = sqlite3_column_text(stmt.get(), 1);
    int merged = sqlite3_column_int(stmt.get(), 2);
    nlohmann::json item;
    item["number"] = number;
    item["title"] = title ? reinterpret_cast<const char *>(title) : "";
    item["merged"] = merged != 0;
    j.push_back(item);
  }
  std::ofstream out(path);
  if (!out) {
    throw std::runtime_error("Failed to open JSON file");
  }
  out << j.dump(2);
}

} // namespace agpm
