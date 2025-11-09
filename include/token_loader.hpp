/**
 * @file token_loader.hpp
 * @brief GitHub token file loader utilities.
 *
 * Declares functions for loading GitHub access tokens from configuration files
 * in various formats (JSON, YAML, TOML) for use by the automation engine.
 */
#ifndef AUTOGITHUBPULLMERGE_TOKEN_LOADER_HPP
#define AUTOGITHUBPULLMERGE_TOKEN_LOADER_HPP

#include <string>
#include <vector>

namespace agpm {

/**
 * Load GitHub access tokens from a configuration file.
 *
 * Supported formats are JSON, YAML, and TOML. Files may contain a flat array
 * of tokens or an object/table with either a single `token` string or a
 * `tokens` array of strings.
 *
 * @param path Filesystem path to the token file
 * @return Vector of tokens discovered in the file
 * @throws std::runtime_error on unsupported formats or read errors
 */
std::vector<std::string> load_tokens_from_file(const std::string &path);

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_TOKEN_LOADER_HPP
