Development Guide
=================

This folder captures developer workflows and scratch assets. Git ignores
everything here by default except this README and `*.example.*` templates.

Building
--------

### Linux

```bash
./scripts/install_linux.sh
cmake --preset vcpkg --fresh
cmake --build --preset vcpkg
```

### macOS

```bash
./scripts/install_mac.sh
cmake --preset vcpkg --fresh
cmake --build --preset vcpkg
```

### Windows

```bat
scripts\install_win.bat
scripts\build_win.bat
```

The Windows build links against the static MSVC runtime to produce a
self-contained executable. The script locates `vswhere.exe` automatically,
initialises the MSVC environment with `VsDevCmd.bat`, and expects the vcpkg
triplet `x64-windows-static`. Install Visual Studio Build Tools before running
the script.

vcpkg Setup
-----------

The helper scripts bootstrap
[vcpkg](https://github.com/microsoft/vcpkg) when `VCPKG_ROOT` is unset, then
install dependencies declared in `vcpkg.json`. On Unix-like hosts the `vcpkg`
CMake preset locates the toolchain automatically; to override it, place a
`CMakeUserPresets.json` in the repo root or export `VCPKG_ROOT` and add it to
`PATH`.

Manual setup steps are below if you prefer to install vcpkg yourself:

### Windows

```powershell
git clone https://github.com/microsoft/vcpkg %USERPROFILE%\vcpkg
%USERPROFILE%\vcpkg\bootstrap-vcpkg.bat
%USERPROFILE%\vcpkg\vcpkg.exe integrate install
setx VCPKG_DEFAULT_TRIPLET x64-windows-static /M
setx VCPKG_ROOT "%USERPROFILE%\vcpkg" /M
setx PATH "%PATH%;%VCPKG_ROOT%" /M
```

### Linux

```bash
git clone https://github.com/microsoft/vcpkg ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
echo "export VCPKG_ROOT=\"$HOME/vcpkg\"" >> ~/.bashrc
echo "export PATH=\"\$VCPKG_ROOT:\$PATH\"" >> ~/.bashrc
```

### macOS

```bash
git clone https://github.com/microsoft/vcpkg ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
echo "export VCPKG_ROOT=\"$HOME/vcpkg\"" >> ~/.zprofile
echo "export PATH=\"\$VCPKG_ROOT:\$PATH\"" >> ~/.zprofile
```

After configuring vcpkg manually, configure and build with the `vcpkg` CMake
preset:

```bash
cmake --preset vcpkg --fresh
cmake --build --preset vcpkg
```

Compiling
---------

The `compile_*` scripts wrap the platform toolchains directly. Make sure the
matching `install_*` script has already bootstrapped vcpkg:

```bash
./scripts/compile_linux.sh   # Linux (g++)
./scripts/compile_mac.sh     # macOS (g++)
./scripts/compile_win.bat    # Windows (MSVC)
```

On Windows the script mirrors `build_win.bat` by setting up MSVC before
invoking `cl.exe`.

Packaging
---------

Packaging is currently a manual step that stages the built binary alongside
documentation. Recommended workflow:

1. Build a fresh Release configuration.
   - Linux/macOS: `cmake --build build/vcpkg --config Release` (the preset uses
     Ninja so Release maps to the default single-config output).
   - Windows: `cmake --build build\vcpkg --config Release`
2. Stage artefacts into `dist/<platform>`:
   - Copy `build/vcpkg/autogithubpullmerge` (Linux/macOS) or
     `build\vcpkg\Release\autogithubpullmerge.exe` (Windows).
   - Include `license.md` and any sample config you want to ship (for example
     `examples/config.example.yaml`).
3. Create the archive.
   - Linux/macOS:

```bash
tar -C dist/linux -czf agpm-linux.tar.gz .
```

   - Windows (PowerShell):

```powershell
Compress-Archive -Path dist\windows\* -DestinationPath agpm-windows.zip
```

Adjust names to match the release version being published.

Run Tests
---------

```bash
ctest --test-dir build -C Debug --output-on-failure -R agpm_tests_core
ctest --test-dir build -C Debug --output-on-failure -R agpm_tests_tui
# CLI tests exist but are disabled by default; enable if needed
```

Format, Lint, Typecheck
-----------------------

```bash
# Format
clang-format -i $(git ls-files | grep -E '\\.(c|cc|cpp|cxx|h|hh|hpp|hxx)$')

# Lint (CI)
clang-tidy -p build $(git ls-files 'src/**/*.cpp') --warnings-as-errors='*'

# Typecheck (CI)
cppcheck --project=build/compile_commands.json \
  --enable=warning,style,performance,portability,information \
  --suppress=missingIncludeSystem --inline-suppr --error-exitcode=2
```

Generate Docs
-------------

```bash
doxygen docs/Doxyfile
```

Local Debugging
---------------

- `run_lldb_commands.example` — LLDB batch commands to run a test and dump
  backtraces on crash. Copy to a local file (ignored by git):

```bash
cp dev/run_lldb_commands.example dev/run_lldb_commands
lldb --batch -s dev/run_lldb_commands ./build/tests/agpm_tests
```

Scratch Space Rules
-------------------

- Keep secrets and throwaway files out of git. If sharing a helpful snippet,
  commit it as a `*.example.*` template.
