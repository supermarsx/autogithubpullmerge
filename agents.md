# Contributor Guidelines

This repository hosts **autogithubpullmerge**, a cross-platform C++23
application that automates merging GitHub pull requests and tidying stale
branches via a terminal UI and scriptable CLI. The project depends on
[`vcpkg`](https://github.com/microsoft/vcpkg) for dependency management,
[`spdlog`](https://github.com/gabime/spdlog) for logging, curses-compatible
libraries for the TUI, Catch2 for unit tests, and Doxygen for API
documentation.

## Authoritative References

- Follow the product requirements in `specs.md` and the long-term direction in
  `roadmap.md` before proposing changes.
- Check `readme.md` for a feature overview, CLI expectations, and supported
  workflows.
- Update the documentation under `docs/` whenever CLI flags, configuration
  files, or notifier behaviour changes.

## Toolchain Expectations

- Target C++23 and keep the existing module layout under `src/`, `include/`,
  and `tests/`.
- Use CMake presets declared in `CMakePresets.json`. The `vcpkg` preset is the
  default for local builds and CI.
- Ensure `vcpkg` dependencies remain consistent with `vcpkg.json` and the
  triplets configured in `vcpkg-configuration.json`.
- The TUI depends on `libncurses-dev` (Linux/macOS) or `pdcurses` (Windows).

## Build and Test Workflow

Before opening a pull request:

1. Bootstrap dependencies (only required once per environment):
   - Linux/macOS:
     ```bash
     ./scripts/install_linux.sh   # or install_mac.sh
     ```
   - Windows:
     ```bat
     scripts\install_win.bat
     ```
2. Configure and build with the shared CMake preset:
   ```bash
   cmake --preset vcpkg --fresh
   cmake --build --preset vcpkg
   ```
   On Windows you may alternatively run `scripts\build_win.bat`.
3. Run the unit tests (add `-C Release` if you built a Release configuration):
   ```bash
   ctest --test-dir build/vcpkg --output-on-failure -R agpm_tests_core
   ctest --test-dir build/vcpkg --output-on-failure -R agpm_tests_tui
   ```
   Enable additional `agpm_tests_cli` targets if your change touches the CLI.
4. Regenerate API docs whenever you modify public headers or user-facing
   interfaces:
   ```bash
   doxygen docs/Doxyfile
   ```

## Formatting, Linting, and Static Analysis

- Format **all** C/C++ sources and headers before committing:
  ```bash
  clang-format -i $(git ls-files | grep -E '\\.(c|cc|cpp|cxx|h|hh|hpp|hxx)$')
  ```
- Address every diagnostic reported by `clang-tidy` (CI treats warnings as
  errors):
  ```bash
  clang-tidy -p build/vcpkg $(git ls-files 'src/**/*.cpp') --warnings-as-errors='*'
  ```
- Keep the static analysis gate clean:
  ```bash
  cppcheck --project=build/vcpkg/compile_commands.json \
    --enable=warning,style,performance,portability,information \
    --suppress=missingIncludeSystem --inline-suppr --error-exitcode=2
  ```
- The CI workflow runs formatting, lint, typecheck, build, and test stages. Ensure
  your changes pass locally before pushing.

## Design and Documentation Notes

- Maintain the separation between the core automation engine, CLI parsing, and
  the TUI. Shared abstractions belong in `include/agpm/` and are exercised via
  tests under `tests/`.
- When adding CLI options or configuration keys, update examples in
  `examples/` and regenerate any reference tables in `docs/`.
- Do not commit secrets or personal configuration. Use `.example` templates in
  `dev/` for shareable snippets.

## Contribution Checklist

Before submitting your work:

- [ ] Specs/roadmap reviewed and documentation updated.
- [ ] Code builds with the `vcpkg` preset on your target platform(s).
- [ ] Tests (`agpm_tests_core`, `agpm_tests_tui`, and any affected suites`) pass.
- [ ] `clang-format`, `clang-tidy`, and `cppcheck` issues resolved.
- [ ] Doxygen regenerated when public headers change.
- [ ] Commit messages and PR description clearly explain the behaviour change.

