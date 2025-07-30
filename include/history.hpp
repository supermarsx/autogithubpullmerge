#ifndef AUTOGITHUBPULLMERGE_HISTORY_HPP
#define AUTOGITHUBPULLMERGE_HISTORY_HPP

#include <nlohmann/json.hpp>
#include <sqlite3.h>
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

  /// Export the database contents to a CSV file.
  void export_csv(const std::string &path);

  /// Export the database contents to a JSON file.
  void export_json(const std::string &path);

private:
  sqlite3 *db_ = nullptr;
};

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_HISTORY_HPP
