#include "log.hpp"
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>
#include <zlib.h>

#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/details/os.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace {
std::weak_ptr<spdlog::logger> g_logger;
std::mutex g_logger_mutex;
std::once_flag g_thread_pool_once;

void ensure_thread_pool() {
  std::call_once(g_thread_pool_once, [] {
    constexpr std::size_t queue_size = 32768;
    constexpr std::size_t num_threads = 1;
    spdlog::init_thread_pool(queue_size, num_threads);
  });
}

namespace fs = std::filesystem;

/**
 * Compute the filesystem path for a rotated log file.
 *
 * @param base Base log file path.
 * @param index Rotation index to compute.
 * @return Filesystem path pointing to the rotated file.
 */
fs::path calc_rotated_path(const std::string &base, std::size_t index) {
  fs::path base_path(base);
  fs::path parent = base_path.parent_path();
  std::string filename = base_path.filename().string();
  if (index == 0) {
    return parent.empty() ? fs::path(filename) : parent / filename;
  }
  std::string basename = filename;
  std::string ext;
  auto pos = filename.find_last_of('.');
  if (pos != std::string::npos && pos != 0) {
    basename = filename.substr(0, pos);
    ext = filename.substr(pos);
  }
  std::string rotated = basename + "." + std::to_string(index) + ext;
  return parent.empty() ? fs::path(rotated) : parent / rotated;
}

/**
 * Rotate compressed log files up to the configured maximum.
 *
 * @param base Base log file path.
 * @param max_files Maximum number of compressed files to retain.
 */
void rotate_compressed_logs(const std::string &base, std::size_t max_files) {
  if (max_files == 0) {
    return;
  }
  auto log = agpm::category_logger("logging");
  log->debug("Rotating compressed logs for '{}' (max {})", base, max_files);
  std::error_code ec;
  fs::path drop = calc_rotated_path(base, max_files);
  fs::remove(drop.string() + ".gz", ec);
  for (std::size_t i = max_files; i > 1; --i) {
    fs::path src_path = calc_rotated_path(base, i - 1);
    fs::path src_gz = fs::path(src_path.string() + ".gz");
    if (!fs::exists(src_gz)) {
      continue;
    }
    fs::path target_path = calc_rotated_path(base, i);
    fs::path target_gz = fs::path(target_path.string() + ".gz");
    fs::remove(target_gz, ec);
    fs::rename(src_gz, target_gz, ec);
  }
}

/**
 * Compress a rotated log file into gzip format.
 *
 * @param path Filesystem path to the log file to compress.
 * @return `true` if compression succeeded, otherwise `false`.
 */
bool compress_rotated_file(const std::string &path) {
  auto log = agpm::category_logger("logging");
  log->debug("Compressing rotated log '{}'", path);
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    log->warn("Failed to open log file {} for compression", path);
    return false;
  }
  std::string compressed_path_str = path + ".gz";
  gzFile gz = gzopen(compressed_path_str.c_str(), "wb");
  if (!gz) {
    log->warn("Failed to open compressed log {}", compressed_path_str);
    return false;
  }
  char buffer[16 * 1024];
  while (input) {
    input.read(buffer, sizeof(buffer));
    std::streamsize read = input.gcount();
    if (read > 0) {
      int written = gzwrite(gz, buffer, static_cast<unsigned>(read));
      if (written == 0 || written != read) {
        int err = 0;
        const char *msg = gzerror(gz, &err);
        log->warn("Failed to compress log {}: {}", path,
                  msg ? msg : "unknown");
        gzclose(gz);
        std::error_code ec;
        fs::remove(fs::path(compressed_path_str), ec);
        return false;
      }
    }
  }
  gzclose(gz);
  input.close();
  std::error_code ec;
  fs::remove(fs::path(path), ec);
  if (ec) {
    log->warn("Failed to remove original log {} after compression: {}", path,
              ec.message());
  }
  log->info("Compressed rotated log '{}'", compressed_path_str);
  return true;
}
} // namespace

namespace agpm {

/**
 * Initialize the global spdlog logger with optional file rotation.
 *
 * @param level Logging verbosity level for the default logger.
 * @param pattern Log message pattern; empty string retains the default.
 * @param file Optional log file path used to enable rotation.
 * @param rotate_files Maximum number of rotated files to keep.
 * @param compress_rotations Whether rotated files should be gzip compressed.
 */
void init_logger(spdlog::level::level_enum level, const std::string &pattern,
                 const std::string &file, std::size_t rotate_files,
                 bool compress_rotations) {
  ensure_thread_pool();
  std::unique_lock<std::mutex> lock(g_logger_mutex);
  auto logger = spdlog::get("agpm");
  if (!logger) {
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    if (!file.empty()) {
      if (rotate_files > 0) {
        spdlog::file_event_handlers handlers;
        if (compress_rotations) {
          handlers.before_open = [rotate_files](
                                     const spdlog::filename_t &filename) {
            const auto base = spdlog::details::os::filename_to_str(filename);
            rotate_compressed_logs(base, rotate_files);
            fs::path newest = calc_rotated_path(base, 1);
            if (fs::exists(newest)) {
              compress_rotated_file(newest.string());
            }
          };
        }
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            file, 1024 * 1024 * 5, rotate_files, false, handlers));
      } else {
        sinks.push_back(
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(file, true));
      }
    }
    auto pool = spdlog::thread_pool();
    if (!pool) {
      ensure_thread_pool();
      pool = spdlog::thread_pool();
    }
    logger = std::make_shared<spdlog::async_logger>(
        "agpm", sinks.begin(), sinks.end(), pool,
        spdlog::async_overflow_policy::block);
    spdlog::set_default_logger(logger);
    g_logger = logger;
  }
  lock.unlock();
  logger->set_level(level);
  if (!pattern.empty()) {
    spdlog::set_pattern(pattern);
  }
  logger->info(
      "Logger initialised (level={}, file='{}', rotate={}, compress={})",
      spdlog::level::to_string_view(level), file, rotate_files,
      compress_rotations ? "true" : "false");
}

/**
 * Ensure that the default logger exists before logging.
 *
 * Creates a new logger when previous initialization was skipped or lost.
 */
void ensure_default_logger() {
  auto logger = spdlog::default_logger();
  auto locked = g_logger.lock();
  if (!logger || !locked || logger.get() != locked.get()) {
    init_logger(spdlog::level::info);
    category_logger("logging")->warn(
        "Default logger missing; reinitialised with info level");
  }
}

std::shared_ptr<spdlog::logger> category_logger(const std::string &category) {
  ensure_thread_pool();
  std::unique_lock<std::mutex> lock(g_logger_mutex);
  auto logger = spdlog::get("agpm." + category);
  if (logger) {
    return logger;
  }
  auto default_logger = spdlog::default_logger();
  if (!default_logger) {
    lock.unlock();
    init_logger(spdlog::level::info);
    lock.lock();
    default_logger = spdlog::default_logger();
  }
  std::vector<spdlog::sink_ptr> sinks;
  if (default_logger) {
    sinks = default_logger->sinks();
  } else {
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
  }
  auto pool = spdlog::thread_pool();
  if (!pool) {
    ensure_thread_pool();
    pool = spdlog::thread_pool();
  }
  auto new_logger = std::make_shared<spdlog::async_logger>(
      "agpm." + category, sinks.begin(), sinks.end(), pool,
      spdlog::async_overflow_policy::block);
  auto level = default_logger ? default_logger->level() : spdlog::level::info;
  new_logger->set_level(level);
  spdlog::register_logger(new_logger);
  return new_logger;
}

void configure_log_categories(
    const std::unordered_map<std::string, spdlog::level::level_enum>
        &overrides) {
  if (overrides.empty()) {
    return;
  }
  for (const auto &[category, level] : overrides) {
    auto logger = category_logger(category);
    logger->set_level(level);
    logger->info("Category '{}' set to level {}", category,
                 spdlog::level::to_string_view(level));
  }
  auto log = category_logger("logging");
  log->info("Applied {} log category override(s)", overrides.size());
}

} // namespace agpm
