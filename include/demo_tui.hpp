/**
 * @file demo_tui.hpp
 * @brief Demo TUI entry point for autogithubpullmerge.
 *
 * Declares the function to run the interactive demo TUI with mock data.
 */

#ifndef AUTOGITHUBPULLMERGE_DEMO_TUI_HPP
#define AUTOGITHUBPULLMERGE_DEMO_TUI_HPP

namespace agpm {

/**
 * Run an interactive demo TUI showing mock pull requests and branches.
 *
 * @return 0 on success, non-zero if the demo could not be displayed.
 */
int run_demo_tui();

} // namespace agpm

#endif // AUTOGITHUBPULLMERGE_DEMO_TUI_HPP
