/**
 * @file history.hpp
 * @brief Pull request history database for autogithubpullmerge.
 *
 * Declares the PullRequestHistory class for storing and exporting PR history
 * using SQLite.
 */

#ifndef AUTOGITHUBPULLMERGE_HISTORY_HPP
#define AUTOGITHUBPULLMERGE_HISTORY_HPP

#include <nlohmann/json.hpp>

#if defined(__CPPCHECK__)
// During static analysis, avoid hard failing if sqlite headers are not
// discoverable. Forward-declare the opaque handle type used in the header.
struct sqlite3;
#elif __has_include(<sqlite3.h>)
#include <sqlite3.h>
#else
// Fallback for environments lacking header discovery; keep declarations usable.
struct sqlite3;
#endif
#include <string>

namespace agpm {

/**
 * Simple RAII wrapper around SQLite for storing pull request history.
 *
 * The class manages opening, migrating, and closing the underlying database
 * connection while exposing a small surface for adding and exporting records.
 */
class PullRequestHistory {
public:
  /**
   * Construct and open the database at `db_path`.
   *
   * @param db_path Filesystem path to the SQLite database file to create or
   *        open. Missing parent directories are not created automatically.
   * @throws std::runtime_error When the database cannot be opened or migrated.
   */
  explicit PullRequestHistory(const std::string &db_path);

  /**
   * Destroy the wrapper and close the database connection if it is open.
   */
  ~PullRequestHistory();

  /**
   * Insert a pull request entry.
   *
   * @param number Numeric pull request number.
   * @param title Pull request title.
   * @param merged Whether the pull request was merged at the time of storage.
   * @throws std::runtime_error When the insert statement fails.
   */
  void insert(int number, const std::string &title, bool merged);

  /**
   * Mark a pull request as merged.
   *
   * @param number Numeric pull request number to mark as merged.
   * @throws std::runtime_error When the update statement fails.
   */
  void update_merged(int number);

  /**
   * Export the database contents to a CSV file.
   *
   * @param path Destination path where the CSV dump should be written.
   * @throws std::runtime_error On I/O failures or when the query execution
   *         fails.
   */
  void export_csv(const std::string &path);

  /**
   * Export the database contents to a JSON file.
   *
   * @param path Destination path where the JSON dump should be written.
   * @throws std::runtime_error On I/O failures or when the query execution
   *         fails.
   */
  void export_json(const std::string &path);

private:
  sqlite3 *db_ = nullptr;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_HISTORY_HPP
