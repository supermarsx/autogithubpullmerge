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

/** Simple wrapper around SQLite for storing pull request history. */
class PullRequestHistory {
public:
  /** Construct and open the database at `db_path`. */
  explicit PullRequestHistory(const std::string &db_path);

  /// Destructor closes the database connection.
  ~PullRequestHistory();

  /**
   * Insert a pull request entry.
   *
   * @param number Pull request number
   * @param title Pull request title
   * @param merged Whether the pull request was merged
   */
  void insert(int number, const std::string &title, bool merged);

  /**
   * Mark a pull request as merged.
   *
   * @param number Pull request number
   */
  void update_merged(int number);

  /// Export the database contents to a CSV file.
  void export_csv(const std::string &path);

  /// Export the database contents to a JSON file.
  void export_json(const std::string &path);

private:
  sqlite3 *db_ = nullptr;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_HISTORY_HPP
