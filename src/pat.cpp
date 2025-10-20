#include "pat.hpp"
#include "log.hpp"

#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>

#if defined(_WIN32)
#include <shellapi.h>
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace agpm {

namespace {
std::shared_ptr<spdlog::logger> pat_log() {
  static auto logger = [] {
    ensure_default_logger();
    return category_logger("pat");
  }();
  return logger;
}
} // namespace

/**
 * Check whether an environment flag is enabled.
 *
 * @param name Environment variable name to inspect.
 * @return `true` when the variable exists and is not "0".
 */
static bool env_flag_enabled(const char *name) {
  if (const char *v = std::getenv(name)) {
    return (*v != '\0' && *v != '0');
  }
  return false;
}

#if defined(__APPLE__)
/**
 * Try opening a URL via LaunchServices without invoking a shell.
 *
 * @param url Target URL to launch.
 * @return `true` on success, otherwise `false`.
 */
static bool ls_open_url(const std::string &url) {
  CFStringRef cfstr = CFStringCreateWithCString(kCFAllocatorDefault, url.c_str(), kCFStringEncodingUTF8);
  if (!cfstr) {
    pat_log()->warn("CFStringCreateWithCString failed");
    return false;
  }
  CFURLRef cfurl = CFURLCreateWithString(kCFAllocatorDefault, cfstr, nullptr);
  CFRelease(cfstr);
  if (!cfurl) {
    pat_log()->warn("CFURLCreateWithString failed");
    return false;
  }
  OSStatus st = LSOpenCFURLRef(cfurl, /*outLaunchedRef*/nullptr);
  CFRelease(cfurl);
  if (st != noErr) {
    pat_log()->warn("LSOpenCFURLRef failed: {}", static_cast<int>(st));
    return false;
  }
  return true;
}

/**
 * Spawn a detached process using posix_spawn.
 *
 * @param argv Command arguments beginning with the executable name.
 * @return `true` when the process was spawned successfully.
 */
static bool spawn_detached(const std::vector<std::string> &argv) {
  std::vector<char*> cargv;
  cargv.reserve(argv.size() + 1);
  for (const auto &s : argv) cargv.push_back(const_cast<char*>(s.c_str()));
  cargv.push_back(nullptr);

  pid_t pid = 0;
  int rc = posix_spawnp(&pid, cargv[0], /*file_actions*/nullptr, /*attrp*/nullptr, cargv.data(), environ);
  if (rc != 0) {
    pat_log()->warn("posix_spawnp('{}') failed: {} ({})", argv[0], std::strerror(rc), rc);
    return false;
  }
  return true; // don't wait; let launchd/OS handle it
}

/**
 * Attempt to open the URL using the BROWSER environment variable.
 *
 * @param url Target URL to launch.
 * @return `true` if a browser command was executed.
 */
static bool try_open_with_browser_env(const std::string &url) {
  if (const char *br = std::getenv("BROWSER")) {
    std::string browser = br;
    if (!browser.empty()) {
      // Use "open -a <App> <url>" so users can force a specific browser by name or path.
      if (spawn_detached({"open", "-a", browser, url})) return true;
    }
  }
  return false;
}

/**
 * Try a series of macOS commands to open the URL directly.
 *
 * @param url Target URL to launch.
 * @return `true` if any command successfully starts a browser.
 */
static bool try_open_cmds(const std::string &url) {
  // Prefer absolute path first to avoid PATH surprises.
  if (spawn_detached({"/usr/bin/open", url})) return true;
  if (spawn_detached({"open", url})) return true;
  // Last resort: AppleScript (still no shell)
  return spawn_detached({"osascript", "-e", std::string("open location \"" + url + "\"")});
}
#endif

/**
 * Launch the GitHub personal access token creation page in a browser.
 *
 * @return `true` when the browser was launched or the action was skipped.
 */
bool open_pat_creation_page() {
  const std::string url = "https://github.com/settings/tokens/new";

  if (env_flag_enabled("AGPM_TEST_SKIP_BROWSER")) {
    return true;
  }

#if defined(_WIN32)
  HINSTANCE res =
      ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
  auto code = reinterpret_cast<uintptr_t>(res);
  if (code > 32) {
    return true;
  }
  pat_log()->warn("ShellExecuteA failed with code {}", code);
  return false;
#elif defined(__APPLE__)
  // On macOS, avoid any shell. Order:
  //   LaunchServices -> $BROWSER via "open -a" -> /usr/bin/open -> PATH "open" -> osascript
  if (ls_open_url(url)) return true;
  if (try_open_with_browser_env(url)) return true;
  if (try_open_cmds(url)) return true;

  pat_log()->warn("Failed to launch default browser for '{}'. Try: /usr/bin/open {}", url, url);
  return false;
#else
  // Linux/Unix: still rely on xdg-open but keep shell usage minimal in the rest of the codebase.
  int rc = std::system((std::string("xdg-open \"") + url + "\" >/dev/null 2>&1 &").c_str());
  if (rc != 0) {
    pat_log()->warn("xdg-open command returned {}", rc);
  }
  return rc == 0;
#endif
}


/**
 * Persist a personal access token to disk with restrictive permissions.
 *
 * @param path_str Destination path for the token file.
 * @param pat Token contents to write.
 * @return `true` on success, otherwise `false`.
 */
bool save_pat_to_file(const std::string &path_str, const std::string &pat) {
  namespace fs = std::filesystem;
  fs::path path(path_str);
  std::error_code ec;
  if (path.has_parent_path()) {
    fs::create_directories(path.parent_path(), ec);
    if (ec) {
      pat_log()->error("Failed to create directories for {}: {}", path_str,
                    ec.message());
      return false;
    }
  }
  std::ofstream out(path, std::ios::out | std::ios::trunc);
  if (!out) {
    pat_log()->error("Failed to open {} for writing", path_str);
    return false;
  }
  out << pat << '\n';
  out.close();
  if (!out) {
    pat_log()->error("Failed to write personal access token to {}", path_str);
    return false;
  }
#ifndef _WIN32
  ::chmod(path.c_str(), S_IRUSR | S_IWUSR);
#endif
  return true;
}

} // namespace agpm
