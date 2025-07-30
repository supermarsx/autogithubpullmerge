#include "app.hpp"
#include "config.hpp"
#include <cassert>
#include <fstream>
#include <vector>

int main() {
  agpm::App app;
  std::vector<char *> args;
  char prog[] = "tests";
  char verbose[] = "--verbose";
  char level[] = "--log-level";
  char level_val[] = "debug";
  char file_opt[] = "--log-file";
  char file_val[] = "test.log";
  args.push_back(prog);
  args.push_back(verbose);
  args.push_back(level);
  args.push_back(level_val);
  args.push_back(file_opt);
  args.push_back(file_val);
  assert(app.run(static_cast<int>(args.size()), args.data()) == 0);
  assert(app.options().verbose);
  assert(app.options().log_level == "debug");
  assert(app.options().log_file == "test.log");

  {
    std::ofstream yaml("test_config.yaml");
    yaml << "verbose: true\nlog_level: debug\nlog_file: cfg.log\n";
    yaml.close();
    agpm::Config cfg = agpm::Config::from_file("test_config.yaml");
    assert(cfg.verbose());
    assert(cfg.log_level() == "debug");
    assert(cfg.log_file() == "cfg.log");
  }

  {
    std::ofstream json("test_config.json");
    json << "{\"verbose\": true, \"log_level\": \"debug\", \"log_file\": "
            "\"cfg.log\"}";
    json.close();
    agpm::Config cfg = agpm::Config::from_file("test_config.json");
    assert(cfg.verbose());
    assert(cfg.log_level() == "debug");
    assert(cfg.log_file() == "cfg.log");
  }
  return 0;
}
