/**
 * @file pat.hpp
 * @brief Personal access token helpers for autogithubpullmerge.
 *
 * Declares helpers for opening the GitHub PAT creation page and saving tokens
 * securely.
 */

#ifndef AUTOGITHUBPULLMERGE_PAT_HPP
#define AUTOGITHUBPULLMERGE_PAT_HPP

#include <string>

namespace agpm {

/**
 * @brief Attempt to open the GitHub PAT creation page in the default browser.
 * @return True on success or when skipped via AGPM_TEST_SKIP_BROWSER, false
 * otherwise.
 */
bool open_pat_creation_page();

/**
 * @brief Persist a personal access token to the given file path.
 *
 * Creates intermediate directories when necessary. The file is truncated before
 * writing and chmod'ed to rw------- on POSIX systems.
 *
 * @param path Filesystem path to save the token.
 * @param pat The personal access token string.
 * @return True on success, false otherwise.
 */
bool save_pat_to_file(const std::string &path, const std::string &pat);

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_PAT_HPP
