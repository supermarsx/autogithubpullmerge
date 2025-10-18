#include "log.hpp"
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>
#include <zlib.h>

#include <spdlog/details/os.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace {
std::weak_ptr<spdlog::logger> g_logger;

namespace fs = std::filesystem;

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

void rotate_compressed_logs(const std::string &base, std::size_t max_files) {
  if (max_files == 0) {
    return;
  }
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

bool compress_rotated_file(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    spdlog::warn("Failed to open log file {} for compression", path);
    return false;
  }
  std::string compressed_path_str = path + ".gz";
  gzFile gz = gzopen(compressed_path_str.c_str(), "wb");
  if (!gz) {
    spdlog::warn("Failed to open compressed log {}", compressed_path_str);
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
        spdlog::warn("Failed to compress log {}: {}", path, msg ? msg : "unknown");
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
    spdlog::warn("Failed to remove original log {} after compression: {}", path,
                 ec.message());
  }
  return true;
}
}

namespace agpm {

void init_logger(spdlog::level::level_enum level, const std::string &pattern,
                 const std::string &file, std::size_t rotate_files,
                 bool compress_rotations) {
  auto logger = spdlog::get("agpm");
  if (!logger) {
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    if (!file.empty()) {
      if (rotate_files > 0) {
        spdlog::file_event_handlers handlers;
        if (compress_rotations) {
          handlers.before_open = [rotate_files](const spdlog::filename_t &filename) {
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
    logger =
        std::make_shared<spdlog::logger>("agpm", sinks.begin(), sinks.end());
    spdlog::set_default_logger(logger);
    g_logger = logger;
  }
  logger->set_level(level);
  if (!pattern.empty()) {
    spdlog::set_pattern(pattern);
  }
}

void ensure_default_logger() {
  auto logger = spdlog::default_logger();
  auto locked = g_logger.lock();
  if (!logger || !locked || logger.get() != locked.get()) {
    init_logger(spdlog::level::info);
  }
}

} // namespace agpm
