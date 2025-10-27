#include "config.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <nlohmann/json.hpp>

TEST_CASE("test config from json") {
  nlohmann::json j;
  auto &core = j["core"];
  core["verbose"] = true;
  core["poll_interval"] = 12;
  core["use_graphql"] = true;

  auto &limits = j["rate_limits"];
  limits["max_request_rate"] = 42;
  limits["max_hourly_requests"] = 3600;

  auto &logging = j["logging"];
  logging["log_level"] = "debug";
  logging["log_limit"] = 210;
  logging["log_rotate"] = 6;
  logging["log_compress"] = true;
  logging["log_categories"] = {{"history", "trace"}, {"http", "debug"}};

  auto &repos = j["repositories"];
  repos["include_repos"] = {"a", "b"};

  auto &workflow = j["workflow"];
  workflow["pr_since"] = "5m";
  workflow["assume_yes"] = true;
  workflow["dry_run"] = true;
  workflow["delete_stray"] = true;
  workflow["heuristic_stray_detection"] = true;
  workflow["stray_detection_engine"] = "both";
  workflow["allow_delete_base_branch"] = true;

  auto &network = j["network"];
  network["http_timeout"] = 40;
  network["http_retries"] = 5;
  network["download_limit"] = 123;
  network["upload_limit"] = 456;
  network["max_download"] = 789;
  network["max_upload"] = 1011;
  network["http_proxy"] = "http://proxy";
  network["https_proxy"] = "http://secureproxy";

  auto &artifacts = j["artifacts"];
  artifacts["export_csv"] = "cfg.csv";
  artifacts["export_json"] = "cfg.json";

  auto &pat = j["personal_access_tokens"];
  pat["open_pat_page"] = false;
  pat["pat_save_path"] = "cfg_pat.txt";
  pat["pat_value"] = "cfg_pat_value";

  auto &single = j["single_run"];
  single["single_open_prs_repo"] = "cfg/open";
  single["single_branches_repo"] = "cfg/branches";

  auto &ui = j["ui"];
  ui["hotkeys"] = {{"enabled", false},
                   {"bindings",
                    {{"refresh", nlohmann::json::array({"Ctrl+R", "r"})},
                     {"merge", nullptr},
                     {"details", "enter"}}}};

  auto &hooks = j["hooks"];
  hooks["enabled"] = true;
  hooks["command"] = "hook_cmd";
  hooks["endpoint"] = "https://hooks.example/json";
  hooks["method"] = "PATCH";
  hooks["headers"] = {{"X-Test", "alpha"}};
  hooks["pull_threshold"] = 12;
  hooks["branch_threshold"] = 3;

  auto &repo_overrides = j["repository_overrides"];
  auto &octo = repo_overrides["octocat/*"];
  octo["only_poll_prs"] = true;
  octo["hooks"]["enabled"] = false;
  octo["hooks"]["actions"] =
      nlohmann::json::array({{{"type", "command"}, {"command", "notify"}}});
  octo["hooks"]["event_actions"]["pull_request.merged"] =
      nlohmann::json::array(
          {{{"type", "http"}, {"endpoint", "https://example.com"},
            {"method", "PUT"}}}});
  auto &regex_override = repo_overrides["regex:^agpm/.+$"];
  regex_override["auto_merge"] = true;

  agpm::Config cfg = agpm::Config::from_json(j);

  REQUIRE(cfg.verbose());
  REQUIRE(cfg.poll_interval() == 12);
  REQUIRE(cfg.max_request_rate() == 42);
  REQUIRE(cfg.max_hourly_requests() == 3600);
  REQUIRE(cfg.log_level() == "debug");
  REQUIRE(cfg.log_limit() == 210);
  REQUIRE(cfg.log_rotate() == 6);
  REQUIRE(cfg.log_compress());
  REQUIRE(cfg.log_categories().at("history") == "trace");
  REQUIRE(cfg.log_categories().at("http") == "debug");
  REQUIRE(cfg.include_repos().size() == 2);
  REQUIRE(cfg.pr_since() == std::chrono::minutes(5));
  REQUIRE(cfg.http_timeout() == 40);
  REQUIRE(cfg.http_retries() == 5);
  REQUIRE(cfg.download_limit() == 123);
  REQUIRE(cfg.upload_limit() == 456);
  REQUIRE(cfg.max_download() == 789);
  REQUIRE(cfg.max_upload() == 1011);
  REQUIRE(cfg.http_proxy() == "http://proxy");
  REQUIRE(cfg.https_proxy() == "http://secureproxy");
  REQUIRE(cfg.use_graphql());
  REQUIRE(cfg.assume_yes());
  REQUIRE(cfg.dry_run());
  REQUIRE(cfg.delete_stray());
  REQUIRE(cfg.heuristic_stray_detection());
  REQUIRE(cfg.stray_detection_mode() == agpm::StrayDetectionMode::Combined);
  REQUIRE(cfg.allow_delete_base_branch());
  REQUIRE(cfg.export_csv() == "cfg.csv");
  REQUIRE(cfg.export_json() == "cfg.json");
  REQUIRE_FALSE(cfg.open_pat_page());
  REQUIRE(cfg.pat_save_path() == "cfg_pat.txt");
  REQUIRE(cfg.pat_value() == "cfg_pat_value");
  REQUIRE(cfg.single_open_prs_repo() == "cfg/open");
  REQUIRE(cfg.single_branches_repo() == "cfg/branches");
  REQUIRE_FALSE(cfg.hotkeys_enabled());
  REQUIRE(cfg.hotkey_bindings().at("refresh") == "Ctrl+R,r");
  REQUIRE(cfg.hotkey_bindings().at("merge").empty());
  REQUIRE(cfg.hotkey_bindings().at("details") == "enter");
  REQUIRE(cfg.hooks_enabled());
  REQUIRE(cfg.hook_command() == "hook_cmd");
  REQUIRE(cfg.hook_endpoint() == "https://hooks.example/json");
  REQUIRE(cfg.hook_method() == "PATCH");
  REQUIRE(cfg.hook_headers().at("X-Test") == "alpha");
  REQUIRE(cfg.hook_pull_threshold() == 12);
  REQUIRE(cfg.hook_branch_threshold() == 3);
  const auto &overrides = cfg.repository_overrides();
  REQUIRE(overrides.size() == 2);
  auto glob_it = std::find_if(overrides.begin(), overrides.end(),
                              [](const agpm::Config::RepositoryOverride &entry) {
                                return entry.pattern == "octocat/*";
                              });
  REQUIRE(glob_it != overrides.end());
  const auto &glob_override = *glob_it;
  REQUIRE(glob_override.actions.has_only_poll_prs);
  REQUIRE(glob_override.actions.only_poll_prs);
  REQUIRE_FALSE(glob_override.actions.has_auto_merge);
  REQUIRE(glob_override.hooks.has_enabled);
  REQUIRE_FALSE(glob_override.hooks.enabled);
  REQUIRE(glob_override.hooks.overrides_default_actions);
  REQUIRE(glob_override.hooks.default_actions.size() == 1);
  REQUIRE(glob_override.hooks.default_actions.front().type ==
          agpm::HookActionType::Command);
  REQUIRE(glob_override.hooks.default_actions.front().command == "notify");
  REQUIRE(glob_override.hooks.overrides_event_actions);
  REQUIRE(glob_override.hooks.event_actions.count("pull_request.merged") == 1);
  const auto &merged_actions =
      glob_override.hooks.event_actions.at("pull_request.merged");
  REQUIRE(merged_actions.size() == 1);
  REQUIRE(merged_actions.front().type == agpm::HookActionType::Http);
  REQUIRE(merged_actions.front().endpoint == "https://example.com");
  REQUIRE(merged_actions.front().method == "PUT");
  auto regex_it = std::find_if(overrides.begin(), overrides.end(),
                               [](const agpm::Config::RepositoryOverride &entry) {
                                 return entry.pattern == "regex:^agpm/.+$";
                               });
  REQUIRE(regex_it != overrides.end());
  REQUIRE(regex_it->actions.has_auto_merge);
  REQUIRE(regex_it->actions.auto_merge);

  const auto *glob_match = cfg.match_repository_override("octocat", "widgets");
  REQUIRE(glob_match != nullptr);
  REQUIRE(glob_match->pattern == "octocat/*");
  const auto *regex_match = cfg.match_repository_override("agpm", "core");
  REQUIRE(regex_match != nullptr);
  REQUIRE(regex_match->pattern == "regex:^agpm/.+$");
  const auto *no_match = cfg.match_repository_override("someone", "else");
  REQUIRE(no_match == nullptr);
}
