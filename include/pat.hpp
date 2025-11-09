/**
 * @file pat.hpp
 * @brief Personal access token helpers for autogithubpullmerge.
 *
 * Declares helpers for opening the GitHub PAT creation page and saving tokens securely.
 */

#ifndef AUTOGITHUBPULLMERGE_PAT_HPP
#define AUTOGITHUBPULLMERGE_PAT_HPP

#include <string>

namespace agpm {

/// Attempt to open the GitHub PAT creation page in the default browser.
/// Returns true on success or when skipped via AGPM_TEST_SKIP_BROWSER.
bool open_pat_creation_page();

/// Persist a personal access token to the given file path, creating
/// intermediate directories when necessary. The file is truncated before
/// writing and chmod'ed to rw------- on POSIX systems.
bool save_pat_to_file(const std::string &path, const std::string &pat);

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_PAT_HPP
