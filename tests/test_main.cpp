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
