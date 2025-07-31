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
  char level_flag[] = "--log-level";
  char level_val[] = "warning";
  args.push_back(prog);
  args.push_back(verbose);
  args.push_back(level_flag);
  args.push_back(level_val);
  assert(app.run(static_cast<int>(args.size()), args.data()) == 0);
  assert(app.options().verbose);
  assert(app.options().log_level == "warning");

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
