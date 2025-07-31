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
  args.push_back(prog);
  args.push_back(verbose);
  assert(app.run(static_cast<int>(args.size()), args.data()) == 0);
  assert(app.options().verbose);

  agpm::App app_cfg;
  std::ofstream cfg("run_config.yaml");
  cfg << "verbose: true\n";
  cfg.close();
  std::vector<char *> args_cfg;
  char prog2[] = "tests";
  char config_flag[] = "--config";
  char cfg_file[] = "run_config.yaml";
  args_cfg.push_back(prog2);
  args_cfg.push_back(config_flag);
  args_cfg.push_back(cfg_file);
  assert(app_cfg.run(static_cast<int>(args_cfg.size()), args_cfg.data()) == 0);
  assert(app_cfg.options().config_file == "run_config.yaml");
  assert(app_cfg.config().verbose());

  agpm::App log_app;
  std::vector<char *> args_log;
  char prog3[] = "tests";
  char log_flag[] = "--log-level";
  char warn_lvl[] = "warn";
  args_log.push_back(prog3);
  args_log.push_back(log_flag);
  args_log.push_back(warn_lvl);
  assert(log_app.run(static_cast<int>(args_log.size()), args_log.data()) == 0);
  assert(log_app.options().log_level == "warn");

  agpm::App hist_app;
  std::vector<char *> hist_args;
  char prog4[] = "tests";
  char db_flag[] = "--history-db";
  char hist_file[] = "hist.db";
  hist_args.push_back(prog4);
  hist_args.push_back(db_flag);
  hist_args.push_back(hist_file);
  assert(hist_app.run(static_cast<int>(hist_args.size()), hist_args.data()) == 0);
  assert(hist_app.options().history_db == "hist.db");

  {
    std::ofstream yaml("test_config.yaml");
    yaml << "verbose: true\n";
    yaml.close();
    agpm::Config cfg = agpm::Config::from_file("test_config.yaml");
    assert(cfg.verbose());
  }

  {
    std::ofstream json("test_config.json");
    json << "{\"verbose\": true}";
    json.close();
    agpm::Config cfg = agpm::Config::from_file("test_config.json");
    assert(cfg.verbose());
  }
  return 0;
}
