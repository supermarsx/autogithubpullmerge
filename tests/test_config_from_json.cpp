#include "config.hpp"
#include <cassert>
#include <chrono>
#include <nlohmann/json.hpp>

int main() {
  nlohmann::json j;
  j["verbose"] = true;
  j["poll_interval"] = 12;
  j["max_request_rate"] = 42;
  j["log_level"] = "debug";
  j["include_repos"] = {"a", "b"};
  j["pr_since"] = "5m";
  j["http_timeout"] = 40;
  j["http_retries"] = 5;

  agpm::Config cfg = agpm::Config::from_json(j);

  assert(cfg.verbose());
  assert(cfg.poll_interval() == 12);
  assert(cfg.max_request_rate() == 42);
  assert(cfg.log_level() == "debug");
  assert(cfg.include_repos().size() == 2);
  assert(cfg.pr_since() == std::chrono::minutes(5));
  assert(cfg.http_timeout() == 40);
  assert(cfg.http_retries() == 5);

  return 0;
}
