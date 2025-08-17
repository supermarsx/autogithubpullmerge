#include "../src/history.cpp"
#include <cassert>
#include <stdexcept>

using namespace agpm;

int main() {
  sqlite3 *db = nullptr;
  assert(sqlite3_open(":memory:", &db) == SQLITE_OK);
  assert(sqlite3_exec(db,
                      "CREATE TABLE pull_requests(id INTEGER PRIMARY KEY, "
                      "number INTEGER, title TEXT, merged INTEGER);",
                      nullptr, nullptr, nullptr) == SQLITE_OK);

  // Exception before stepping
  try {
    Statement stmt(
        db, "INSERT INTO pull_requests(number,title,merged) VALUES(1,'t',0)");
    throw std::runtime_error("boom");
  } catch (const std::runtime_error &) {
  }
  assert(sqlite3_next_stmt(db, nullptr) == nullptr);

  // Exception after stepping
  try {
    Statement stmt(
        db, "INSERT INTO pull_requests(number,title,merged) VALUES(2,'t',0)");
    sqlite3_step(stmt.get());
    throw std::runtime_error("boom");
  } catch (const std::runtime_error &) {
  }
  assert(sqlite3_next_stmt(db, nullptr) == nullptr);

  sqlite3_close(db);
  return 0;
}
