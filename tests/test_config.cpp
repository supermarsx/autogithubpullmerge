#include "config.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>

TEST_CASE("test config") {
  // YAML config with extended options
  {
    std::ofstream f("cfg.yaml");
    f << "core:\n";
    f << "  verbose: true\n";
    f << "  poll_interval: 3\n";
    f << "rate_limits:\n";
    f << "  max_request_rate: 10\n";
    f << "  max_hourly_requests: 2400\n";
    f << "  rate_limit_margin: 0.6\n";
    f << "  rate_limit_refresh_interval: 75\n";
    f << "  retry_rate_limit_endpoint: true\n";
    f << "  rate_limit_retry_limit: 4\n";
    f << "logging:\n";
    f << "  log_level: debug\n";
    f << "  log_limit: 150\n";
    f << "  log_rotate: 5\n";
    f << "  log_compress: true\n";
    f << "  log_categories:\n";
    f << "    history: trace\n";
    f << "    http: debug\n";
    f << "repositories:\n";
    f << "  include_repos:\n";
    f << "    - repoA\n";
    f << "    - repoB\n";
    f << "  exclude_repos:\n";
    f << "    - repoC\n";
    f << "  include_merged: true\n";
    f << "  repo_discovery_mode: all\n";
    f << "  repo_discovery_roots:\n";
    f << "    - /tmp/repos\n";
    f << "tokens:\n";
    f << "  api_keys:\n";
    f << "    - a\n";
    f << "    - b\n";
    f << "artifacts:\n";
    f << "  history_db: hist.db\n";
    f << "  export_csv: export.csv\n";
    f << "  export_json: export.json\n";
    f << "workflow:\n";
    f << "  assume_yes: true\n";
    f << "  dry_run: true\n";
    f << "  only_poll_prs: true\n";
    f << "  reject_dirty: true\n";
    f << "  delete_stray: true\n";
    f << "  heuristic_stray_detection: true\n";
    f << "  auto_merge: true\n";
    f << "  allow_delete_base_branch: true\n";
    f << "  purge_only: true\n";
    f << "  purge_prefix: tmp/\n";
    f << "  pr_limit: 25\n";
    f << "  pr_since: 2h\n";
    f << "  sort: reverse\n";
    f << "hooks:\n";
    f << "  enabled: true\n";
    f << "  command: notify.sh\n";
    f << "  endpoint: https://hooks.example/notify\n";
    f << "  method: POST\n";
    f << "  headers:\n";
    f << "    X-Token: secret\n";
    f << "  pull_threshold: 25\n";
    f << "  branch_threshold: 10\n";
    f << "network:\n";
    f << "  download_limit: 1000\n";
    f << "  upload_limit: 2000\n";
    f << "  max_download: 3000\n";
    f << "  max_upload: 4000\n";
    f << "  http_proxy: http://proxy\n";
    f << "  https_proxy: http://secureproxy\n";
    f << "personal_access_tokens:\n";
    f << "  open_pat_page: true\n";
    f << "  pat_save_path: pat.txt\n";
    f << "  pat_value: config_pat\n";
    f << "single_run:\n";
    f << "  single_open_prs_repo: owner/repo\n";
    f << "  single_branches_repo: owner/repo\n";
    f << "ui:\n";
    f << "  hotkeys:\n";
    f << "    enabled: false\n";
    f << "    bindings:\n";
    f << "      refresh:\n";
    f << "        - Ctrl+R\n";
    f << "        - r\n";
    f << "      merge: null\n";
    f << "      details: \"enter|d\"\n";
    f.close();
  }
  agpm::Config yaml_cfg = agpm::Config::from_file("cfg.yaml");
  REQUIRE(yaml_cfg.verbose());
  REQUIRE(yaml_cfg.poll_interval() == 3);
  REQUIRE(yaml_cfg.max_request_rate() == 10);
  REQUIRE(yaml_cfg.max_hourly_requests() == 2400);
  REQUIRE(yaml_cfg.rate_limit_margin() == Catch::Approx(0.6));
  REQUIRE(yaml_cfg.rate_limit_refresh_interval() == 75);
  REQUIRE(yaml_cfg.retry_rate_limit_endpoint());
  REQUIRE(yaml_cfg.rate_limit_retry_limit() == 4);
  REQUIRE(yaml_cfg.log_level() == "debug");
  REQUIRE(yaml_cfg.log_limit() == 150);
  REQUIRE(yaml_cfg.log_rotate() == 5);
  REQUIRE(yaml_cfg.log_compress());
  REQUIRE(yaml_cfg.log_categories().at("history") == "trace");
  REQUIRE(yaml_cfg.log_categories().at("http") == "debug");
  REQUIRE(yaml_cfg.include_repos().size() == 2);
  REQUIRE(yaml_cfg.include_repos()[0] == "repoA");
  REQUIRE(yaml_cfg.exclude_repos().size() == 1);
  REQUIRE(yaml_cfg.exclude_repos()[0] == "repoC");
  REQUIRE(yaml_cfg.api_keys().size() == 2);
  REQUIRE(yaml_cfg.include_merged());
  REQUIRE(yaml_cfg.repo_discovery_mode() == agpm::RepoDiscoveryMode::All);
  REQUIRE(yaml_cfg.repo_discovery_roots().size() == 1);
  REQUIRE(yaml_cfg.repo_discovery_roots()[0] == "/tmp/repos");
  REQUIRE(yaml_cfg.history_db() == "hist.db");
  REQUIRE(yaml_cfg.export_csv() == "export.csv");
  REQUIRE(yaml_cfg.export_json() == "export.json");
  REQUIRE(yaml_cfg.assume_yes());
  REQUIRE(yaml_cfg.dry_run());
  REQUIRE(yaml_cfg.only_poll_prs());
  REQUIRE(yaml_cfg.reject_dirty());
  REQUIRE(yaml_cfg.delete_stray());
  REQUIRE(yaml_cfg.heuristic_stray_detection());
  REQUIRE(yaml_cfg.stray_detection_mode() ==
          agpm::StrayDetectionMode::Combined);
  REQUIRE(yaml_cfg.auto_merge());
  REQUIRE(yaml_cfg.allow_delete_base_branch());
  REQUIRE(yaml_cfg.purge_only());
  REQUIRE(yaml_cfg.purge_prefix() == "tmp/");
  REQUIRE(yaml_cfg.pr_limit() == 25);
  REQUIRE(yaml_cfg.pr_since() == std::chrono::hours(2));
  REQUIRE(yaml_cfg.sort_mode() == "reverse");
  REQUIRE(yaml_cfg.hooks_enabled());
  REQUIRE(yaml_cfg.hook_command() == "notify.sh");
  REQUIRE(yaml_cfg.hook_endpoint() == "https://hooks.example/notify");
  REQUIRE(yaml_cfg.hook_method() == "POST");
  REQUIRE(yaml_cfg.hook_headers().at("X-Token") == "secret");
  REQUIRE(yaml_cfg.hook_pull_threshold() == 25);
  REQUIRE(yaml_cfg.hook_branch_threshold() == 10);
  REQUIRE(yaml_cfg.download_limit() == 1000);
  REQUIRE(yaml_cfg.upload_limit() == 2000);
  REQUIRE(yaml_cfg.max_download() == 3000);
  REQUIRE(yaml_cfg.max_upload() == 4000);
  REQUIRE(yaml_cfg.http_proxy() == "http://proxy");
  REQUIRE(yaml_cfg.https_proxy() == "http://secureproxy");
  REQUIRE(yaml_cfg.open_pat_page());
  REQUIRE(yaml_cfg.pat_save_path() == "pat.txt");
  REQUIRE(yaml_cfg.pat_value() == "config_pat");
  REQUIRE(yaml_cfg.single_open_prs_repo() == "owner/repo");
  REQUIRE(yaml_cfg.single_branches_repo() == "owner/repo");
  REQUIRE_FALSE(yaml_cfg.hotkeys_enabled());
  REQUIRE(yaml_cfg.hotkey_bindings().at("refresh") == "Ctrl+R,r");
  REQUIRE(yaml_cfg.hotkey_bindings().at("merge").empty());
  REQUIRE(yaml_cfg.hotkey_bindings().at("details") == "enter|d");

  // JSON config with extended options
  {
    nlohmann::json doc;
    auto &core = doc["core"];
    core["verbose"] = false;
    core["poll_interval"] = 2;

    auto &limits = doc["rate_limits"];
    limits["max_request_rate"] = 5;
    limits["max_hourly_requests"] = 2600;
    limits["rate_limit_margin"] = 0.5;
    limits["rate_limit_refresh_interval"] = 45;
    limits["retry_rate_limit_endpoint"] = false;
    limits["rate_limit_retry_limit"] = 2;

    auto &logging = doc["logging"];
    logging["log_level"] = "warn";
    logging["log_limit"] = 175;
    logging["log_rotate"] = 2;
    logging["log_compress"] = true;
    logging["log_categories"] = {{"history", "trace"}, {"http", "debug"}};

    auto &repos = doc["repositories"];
    repos["include_repos"] = {"x"};
    repos["exclude_repos"] = {"y", "z"};
    repos["repo_discovery_mode"] = "disabled";
    repos["repo_discovery_roots"] = {"./repos"};

    auto &tokens = doc["tokens"];
    tokens["api_keys"] = {"k1"};

    auto &artifacts = doc["artifacts"];
    artifacts["history_db"] = "db.sqlite";
    artifacts["export_csv"] = "out.csv";
    artifacts["export_json"] = "out.json";

    auto &workflow = doc["workflow"];
    workflow["assume_yes"] = false;
    workflow["dry_run"] = true;
    workflow["only_poll_stray"] = true;
    workflow["purge_only"] = true;
    workflow["purge_prefix"] = "test/";
    workflow["pr_limit"] = 30;
    workflow["pr_since"] = "15m";
    workflow["sort"] = "alphanum";
    workflow["delete_stray"] = false;
    workflow["heuristic_stray_detection"] = true;
    workflow["stray_detection_engine"] = "heuristic";
    workflow["allow_delete_base_branch"] = false;

    auto &network = doc["network"];
    network["download_limit"] = 500;
    network["upload_limit"] = 600;
    network["max_download"] = 700;
    network["max_upload"] = 800;
    network["http_proxy"] = "http://proxy";
    network["https_proxy"] = "http://secureproxy";

    auto &pat = doc["personal_access_tokens"];
    pat["open_pat_page"] = false;
    pat["pat_save_path"] = "";
    pat["pat_value"] = "";

    auto &single = doc["single_run"];
    single["single_open_prs_repo"] = "single/repo";
    single["single_branches_repo"] = "single/repo";

    auto &ui = doc["ui"];
    ui["hotkeys"] = {
        {"enabled", true},
        {"bindings",
         {{"open", "o"}, {"quit", nlohmann::json::array({"Ctrl+Q", "q"})}}}};

    auto &hooks = doc["hooks"];
    hooks["enabled"] = true;
    hooks["command"] = "json_cmd";
    hooks["endpoint"] = "https://json.example/hook";
    hooks["method"] = "PUT";
    hooks["headers"] = {{"Authorization", "Bearer abc"}};
    hooks["pull_threshold"] = 15;
    hooks["branch_threshold"] = 7;

    std::ofstream f("cfg.json");
    f << doc.dump();
  }
  agpm::Config json_cfg = agpm::Config::from_file("cfg.json");
  REQUIRE(!json_cfg.verbose());
  REQUIRE(json_cfg.poll_interval() == 2);
  REQUIRE(json_cfg.max_request_rate() == 5);
  REQUIRE(json_cfg.max_hourly_requests() == 2600);
  REQUIRE(json_cfg.rate_limit_margin() == Catch::Approx(0.5));
  REQUIRE(json_cfg.rate_limit_refresh_interval() == 45);
  REQUIRE_FALSE(json_cfg.retry_rate_limit_endpoint());
  REQUIRE(json_cfg.rate_limit_retry_limit() == 2);
  REQUIRE(json_cfg.log_level() == "warn");
  REQUIRE(json_cfg.log_limit() == 175);
  REQUIRE(json_cfg.log_rotate() == 2);
  REQUIRE(json_cfg.log_compress());
  REQUIRE(json_cfg.log_categories().at("history") == "trace");
  REQUIRE(json_cfg.log_categories().at("http") == "debug");
  REQUIRE(json_cfg.include_repos().size() == 1);
  REQUIRE(json_cfg.include_repos()[0] == "x");
  REQUIRE(json_cfg.exclude_repos().size() == 2);
  REQUIRE(json_cfg.exclude_repos()[1] == "z");
  REQUIRE(json_cfg.api_keys().size() == 1);
  REQUIRE(json_cfg.repo_discovery_mode() == agpm::RepoDiscoveryMode::Disabled);
  REQUIRE(json_cfg.repo_discovery_roots().size() == 1);
  REQUIRE(json_cfg.repo_discovery_roots()[0] == "./repos");
  REQUIRE(json_cfg.history_db() == "db.sqlite");
  REQUIRE(json_cfg.export_csv() == "out.csv");
  REQUIRE(json_cfg.export_json() == "out.json");
  REQUIRE(json_cfg.dry_run());
  REQUIRE_FALSE(json_cfg.assume_yes());
  REQUIRE(json_cfg.only_poll_stray());
  REQUIRE(json_cfg.heuristic_stray_detection());
  REQUIRE(json_cfg.stray_detection_mode() ==
          agpm::StrayDetectionMode::Heuristic);
  REQUIRE(json_cfg.purge_only());
  REQUIRE(json_cfg.purge_prefix() == "test/");
  REQUIRE(json_cfg.pr_limit() == 30);
  REQUIRE(json_cfg.pr_since() == std::chrono::minutes(15));
  REQUIRE(json_cfg.sort_mode() == "alphanum");
  REQUIRE(json_cfg.download_limit() == 500);
  REQUIRE(json_cfg.upload_limit() == 600);
  REQUIRE(json_cfg.max_download() == 700);
  REQUIRE(json_cfg.max_upload() == 800);
  REQUIRE(json_cfg.http_proxy() == "http://proxy");
  REQUIRE(json_cfg.https_proxy() == "http://secureproxy");
  REQUIRE_FALSE(json_cfg.delete_stray());
  REQUIRE_FALSE(json_cfg.allow_delete_base_branch());
  REQUIRE_FALSE(json_cfg.open_pat_page());
  REQUIRE(json_cfg.pat_save_path().empty());
  REQUIRE(json_cfg.pat_value().empty());
  REQUIRE(json_cfg.single_open_prs_repo() == "single/repo");
  REQUIRE(json_cfg.single_branches_repo() == "single/repo");
  REQUIRE(json_cfg.hotkeys_enabled());
  REQUIRE(json_cfg.hotkey_bindings().at("open") == "o");
  REQUIRE(json_cfg.hotkey_bindings().at("quit") == "Ctrl+Q,q");
  REQUIRE(json_cfg.hooks_enabled());
  REQUIRE(json_cfg.hook_command() == "json_cmd");
  REQUIRE(json_cfg.hook_endpoint() == "https://json.example/hook");
  REQUIRE(json_cfg.hook_method() == "PUT");
  REQUIRE(json_cfg.hook_headers().at("Authorization") == "Bearer abc");
  REQUIRE(json_cfg.hook_pull_threshold() == 15);
  REQUIRE(json_cfg.hook_branch_threshold() == 7);

  // TOML config with extended options
  {
    std::ofstream f("cfg.toml");
    f << "[core]\n";
    f << "verbose = true\n";
    f << "poll_interval = 8\n";
    f << "use_graphql = true\n\n";

    f << "[rate_limits]\n";
    f << "max_request_rate = 12\n";
    f << "max_hourly_requests = 2800\n\n";

    f << "[logging]\n";
    f << "log_level = \"info\"\n";
    f << "log_limit = 220\n";
    f << "log_rotate = 4\n";
    f << "log_compress = true\n\n";

    f << "[logging.log_categories]\n";
    f << "history = \"trace\"\n";
    f << "http = \"debug\"\n\n";

    f << "[repositories]\n";
    f << "include_repos = [\"repoTomlA\", \"repoTomlB\"]\n";
    f << "exclude_repos = [\"repoTomlC\"]\n\n";

    f << "[tokens]\n";
    f << "api_keys = [\"tok1\", \"tok2\"]\n\n";

    f << "[artifacts]\n";
    f << "history_db = \"history_toml.db\"\n\n";

    f << "[workflow]\n";
    f << "only_poll_stray = true\n";
    f << "heuristic_stray_detection = false\n";
    f << "stray_detection_engine = \"rule\"\n";
    f << "purge_only = false\n";
    f << "purge_prefix = \"hotfix/\"\n";
    f << "pr_limit = 15\n";
    f << "pr_since = \"45m\"\n";
    f << "sort = \"reverse-alphanum\"\n\n";

    f << "[network]\n";
    f << "download_limit = 1500\n";
    f << "upload_limit = 1600\n";
    f << "max_download = 1700\n";
    f << "max_upload = 1800\n";
    f << "http_timeout = 45\n";
    f << "http_retries = 6\n";
    f.close();
  }
  agpm::Config toml_cfg = agpm::Config::from_file("cfg.toml");
  REQUIRE(toml_cfg.verbose());
  REQUIRE(toml_cfg.poll_interval() == 8);
  REQUIRE(toml_cfg.max_request_rate() == 12);
  REQUIRE(toml_cfg.max_hourly_requests() == 2800);
  REQUIRE(toml_cfg.log_level() == "info");
  REQUIRE(toml_cfg.log_limit() == 220);
  REQUIRE(toml_cfg.log_rotate() == 4);
  REQUIRE(toml_cfg.log_compress());
  REQUIRE(toml_cfg.log_categories().at("history") == "trace");
  REQUIRE(toml_cfg.log_categories().at("http") == "debug");
  REQUIRE(toml_cfg.include_repos().size() == 2);
  REQUIRE(toml_cfg.include_repos()[1] == "repoTomlB");
  REQUIRE(toml_cfg.exclude_repos().size() == 1);
  REQUIRE(toml_cfg.exclude_repos()[0] == "repoTomlC");
  REQUIRE(toml_cfg.api_keys().size() == 2);
  REQUIRE(toml_cfg.history_db() == "history_toml.db");
  REQUIRE(toml_cfg.only_poll_stray());
  REQUIRE_FALSE(toml_cfg.heuristic_stray_detection());
  REQUIRE(toml_cfg.stray_detection_mode() ==
          agpm::StrayDetectionMode::RuleBased);
  REQUIRE(!toml_cfg.purge_only());
  REQUIRE(toml_cfg.purge_prefix() == "hotfix/");
  REQUIRE(toml_cfg.pr_limit() == 15);
  REQUIRE(toml_cfg.pr_since() == std::chrono::minutes(45));
  REQUIRE(toml_cfg.sort_mode() == "reverse-alphanum");
  REQUIRE(toml_cfg.download_limit() == 1500);
  REQUIRE(toml_cfg.upload_limit() == 1600);
  REQUIRE(toml_cfg.max_download() == 1700);
  REQUIRE(toml_cfg.max_upload() == 1800);
  REQUIRE(toml_cfg.http_timeout() == 45);
  REQUIRE(toml_cfg.http_retries() == 6);
  REQUIRE(toml_cfg.use_graphql());

  // Token files referenced from config
  {
    std::ofstream y("tokens.yaml");
    y << "- ytok1\n- ytok2\n";
    y.close();
    std::ofstream t("tokens.toml");
    t << "token = \"ttok\"\n";
    t.close();
    std::ofstream cfg("cfg_tokens.json");
    cfg << R"({"tokens":{"api_keys":["direct"],"api_key_files":["tokens.yaml","tokens.toml"]}})";
    cfg.close();
    agpm::Config cfg_tokens = agpm::Config::from_file("cfg_tokens.json");
    REQUIRE(cfg_tokens.api_keys().size() == 4);
    REQUIRE(cfg_tokens.api_keys()[0] == "direct");
    REQUIRE(cfg_tokens.api_keys()[1] == "ytok1");
    REQUIRE(cfg_tokens.api_keys()[2] == "ytok2");
    REQUIRE(cfg_tokens.api_keys()[3] == "ttok");
  }

  {
    std::ofstream cfg("cfg_root.json");
    cfg << R"({"repositories":{"repo_discovery_root":"/var/repos","repo_discovery_mode":"filesystem"}})";
    cfg.close();
    agpm::Config cfg_root = agpm::Config::from_file("cfg_root.json");
    REQUIRE(cfg_root.repo_discovery_mode() ==
            agpm::RepoDiscoveryMode::Filesystem);
    REQUIRE(cfg_root.repo_discovery_roots().size() == 1);
    REQUIRE(cfg_root.repo_discovery_roots()[0] == "/var/repos");
  }

  {
    std::ofstream cfg("cfg_both.json");
    cfg << R"({"repositories":{"repo_discovery_mode":"both","repo_discovery_roots":["/var/repos","/srv/repos"]}})";
    cfg.close();
    agpm::Config cfg_both = agpm::Config::from_file("cfg_both.json");
    REQUIRE(cfg_both.repo_discovery_mode() == agpm::RepoDiscoveryMode::Both);
    REQUIRE(cfg_both.repo_discovery_roots().size() == 2);
    REQUIRE(cfg_both.repo_discovery_roots()[0] == "/var/repos");
    REQUIRE(cfg_both.repo_discovery_roots()[1] == "/srv/repos");
  }
}
