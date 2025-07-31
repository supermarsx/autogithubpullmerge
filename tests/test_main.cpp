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
