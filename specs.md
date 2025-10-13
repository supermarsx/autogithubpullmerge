Product specification list

- It's a C++ app that has a fluid TUI interface to manage and monitor pull requests
- its cross platform compatible with win, linux and macos
- Has extensive cli args for all configurations
- Has adequate tests for every piece of code
- Code is well documented using doxygen
- commits follow "conventional commits structure"
- Allows to use yaml configuration file
- Allows to use json configuration file
- Has extensive logging capabilities throughout the code
- Provides install dependencies scripts for win, linux and macos
- Provides compilation scripts for win, linux and macos
- Dependencies managed via vcpkg manifest (vcpkg.json); no vendored
  third‑party libraries are committed to the repo. The `libs/` directory is
  ignored.
- CMake optionally fetches fresh dependencies when configured with
  `-DAGPM_FETCH_DEPS=ON` (requires `VCPKG_ROOT` to point to a vcpkg install).
- Provides extensive logging facilities and configurations
- Has an updated readme with every feature, config and so on
- has extensive rules configurations for merging
- has options for protected branches, including, excluding, patterns, regex, and others
- has options for polling intervals
- has hotkey functions for when the window is focused
- implements github library using curl lib
- has multithreaded configurations for polling, merging etc
- has rate limiting configs to prevent too many requests
- can base tui in supermarsx/autogitpull implementation, for fluidity
- can export or log pull requests to csv or json file as they come and are resolved
- can use embedded sqlite to request data and so on
- add tui colors to help tracking and visually accompany whats happening
- add cli arg to override dirty branches
- add cli arg to auto delete stray branches
- cli has a isolated stray branch purge mode, and an integrated stray branch deletion mode
- Has guards in all functions to avoid memory leaks
- has cli args to limit download and upload speed
- has cli args to limit download and upload cumulative traffic
- has cli arg sorting mode alpha, reverse, alphanum, reverse alphanum
- has cli arg to exclude repo, its repeatable
- has cli arg to include repo, its repeatable
- by default checks for all repos available to a given api key
- has cli mode to use global mode or individual repo/multiple repo mode
- compilation uses g++ on Linux and macOS, and MSVC on Windows
- use the latest c++
- ensure compilation uses good optimizations

Contributor notes
- Do not commit third‑party code under `libs/`. Use the vcpkg manifest to add
  or update dependencies.
- If removing previously vendored code, purge it from git tracking with
  `git rm -r --cached libs` (then commit), and keep `libs/` in `.gitignore`.
