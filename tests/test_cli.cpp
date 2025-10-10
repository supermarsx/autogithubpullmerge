#include "cli.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>

TEST_CASE("test cli") {
  char prog[] = "prog";
  char verbose[] = "--verbose";
  char *argv1[] = {prog, verbose};
  agpm::CliOptions opts = agpm::parse_cli(2, argv1);
  REQUIRE(opts.verbose);

  char *argv2[] = {prog};
  agpm::CliOptions opts2 = agpm::parse_cli(1, argv2);
  REQUIRE(!opts2.verbose);

  char config_flag[] = "--config";
  char file[] = "cfg.yaml";
  char *argv3[] = {prog, config_flag, file};
  agpm::CliOptions opts3 = agpm::parse_cli(3, argv3);
  REQUIRE(opts3.config_file == "cfg.yaml");

  char log_flag[] = "--log-level";
  char debug_lvl[] = "debug";
  char *argv4[] = {prog, log_flag, debug_lvl};
  agpm::CliOptions opts4 = agpm::parse_cli(3, argv4);
  REQUIRE(opts4.log_level == "debug");

  char log_file_flag[] = "--log-file";
  char path[] = "app.log";
  char *argv4b[] = {prog, log_file_flag, path};
  agpm::CliOptions opts4b = agpm::parse_cli(3, argv4b);
  REQUIRE(opts4b.log_file == "app.log");

  char log_limit_flag[] = "--log-limit";
  char log_limit_val[] = "123";
  char *argv4c[] = {prog, log_limit_flag, log_limit_val};
  agpm::CliOptions opts4c = agpm::parse_cli(3, argv4c);
  REQUIRE(opts4c.log_limit == 123);

  char *argv5[] = {prog};
  agpm::CliOptions opts5 = agpm::parse_cli(1, argv5);
  REQUIRE(opts5.log_level == "info");
  REQUIRE(!opts5.include_merged);
  REQUIRE(opts5.log_limit == 200);

  char include_flag[] = "--include";
  char repo_a[] = "repoA";
  char repo_b[] = "repoB";
  char *argv6[] = {prog, include_flag, repo_a, include_flag, repo_b};
  agpm::CliOptions opts6 = agpm::parse_cli(5, argv6);
  REQUIRE(opts6.include_repos.size() == 2);
  REQUIRE(opts6.include_repos[0] == "repoA");
  REQUIRE(opts6.include_repos[1] == "repoB");

  char exclude_flag[] = "--exclude";
  char repo_c[] = "repoC";
  char *argv7[] = {prog, exclude_flag, repo_c};
  agpm::CliOptions opts7 = agpm::parse_cli(3, argv7);
  REQUIRE(opts7.exclude_repos.size() == 1);
  REQUIRE(opts7.exclude_repos[0] == "repoC");

  char repo_d[] = "repoD";
  char repo_e[] = "repoE";
  char *argv7b[] = {prog, exclude_flag, repo_d, exclude_flag, repo_e};
  agpm::CliOptions opts7b = agpm::parse_cli(5, argv7b);
  REQUIRE(opts7b.exclude_repos.size() == 2);
  REQUIRE(opts7b.exclude_repos[0] == "repoD");
  REQUIRE(opts7b.exclude_repos[1] == "repoE");

  char protect_flag[] = "--protect-branch";
  char protect_ex_flag[] = "--protect-branch-exclude";
  char pat_a[] = "main";
  char pat_b[] = "main-temp";
  char *argv_protect[] = {prog, protect_flag, pat_a, protect_ex_flag, pat_b};
  agpm::CliOptions opts_protect = agpm::parse_cli(5, argv_protect);
  REQUIRE(opts_protect.protected_branches.size() == 1);
  REQUIRE(opts_protect.protected_branches[0] == "main");
  REQUIRE(opts_protect.protected_branch_excludes.size() == 1);
  REQUIRE(opts_protect.protected_branch_excludes[0] == "main-temp");

  char api_flag[] = "--api-key";
  char key1[] = "abc";
  char key2[] = "def";
  char *argv8[] = {prog, api_flag, key1, api_flag, key2};
  agpm::CliOptions opts8 = agpm::parse_cli(5, argv8);
  REQUIRE(opts8.api_keys.size() == 2);
  REQUIRE(opts8.api_keys[0] == "abc");
  REQUIRE(opts8.api_keys[1] == "def");

  {
    std::ofstream f("tok.yaml");
    f << "tokens:\n  - a\n  - b\n";
    f.close();
    char file_flag[] = "--api-key-file";
    char path[] = "tok.yaml";
    char *argv9[] = {prog, file_flag, path};
    agpm::CliOptions opts9 = agpm::parse_cli(3, argv9);
    REQUIRE(opts9.api_keys.size() == 2);
    REQUIRE(opts9.api_keys[0] == "a");
    REQUIRE(opts9.api_keys[1] == "b");
  }

  // Environment variable fallback
  {
    setenv("GITHUB_TOKEN", "envtok", 1);
    char *argv_env[] = {prog};
    agpm::CliOptions opts_env = agpm::parse_cli(1, argv_env);
    REQUIRE(opts_env.api_keys.size() == 1);
    REQUIRE(opts_env.api_keys[0] == "envtok");
    unsetenv("GITHUB_TOKEN");
  }

  // Environment variable ignored when tokens supplied
  {
    setenv("GITHUB_TOKEN", "envtok", 1);
    char api_flag2[] = "--api-key";
    char key_env[] = "cmdtok";
    char *argv_env2[] = {prog, api_flag2, key_env};
    agpm::CliOptions opts_env2 = agpm::parse_cli(3, argv_env2);
    REQUIRE(opts_env2.api_keys.size() == 1);
    REQUIRE(opts_env2.api_keys[0] == "cmdtok");
    unsetenv("GITHUB_TOKEN");
  }

  char stream_flag[] = "--api-key-from-stream";
  char *argv10[] = {prog, stream_flag};
  agpm::CliOptions opts10 = agpm::parse_cli(2, argv10);
  REQUIRE(opts10.api_key_from_stream);

  char interval_flag[] = "--poll-interval";
  char rate_flag[] = "--max-request-rate";
  char interval_val[] = "5";
  char rate_val[] = "100";
  char *argv11[] = {prog, interval_flag, interval_val, rate_flag, rate_val};
  agpm::CliOptions opts11 = agpm::parse_cli(5, argv11);
  REQUIRE(opts11.poll_interval == 5);
  REQUIRE(opts11.max_request_rate == 100);

  char db_flag[] = "--history-db";
  char path_db[] = "my.db";
  char *argv12[] = {prog, db_flag, path_db};
  agpm::CliOptions opts12 = agpm::parse_cli(3, argv12);
  REQUIRE(opts12.history_db == "my.db");

  char merged_flag[] = "--include-merged";
  char *argv13[] = {prog, merged_flag};
  agpm::CliOptions opts13 = agpm::parse_cli(2, argv13);
  REQUIRE(opts13.include_merged);

  char poll_prs_flag[] = "--only-poll-prs";
  char *argv14[] = {prog, poll_prs_flag};
  agpm::CliOptions opts14 = agpm::parse_cli(2, argv14);
  REQUIRE(opts14.only_poll_prs);

  char poll_stray_flag[] = "--only-poll-stray";
  char *argv15[] = {prog, poll_stray_flag};
  agpm::CliOptions opts15 = agpm::parse_cli(2, argv15);
  REQUIRE(opts15.only_poll_stray);

  char yes_flag[] = "--yes";
  char reject_flag[] = "--reject-dirty";
  char *argv16b[] = {prog, yes_flag, reject_flag};
  agpm::CliOptions opts16b = agpm::parse_cli(3, argv16b);
  REQUIRE(opts16b.reject_dirty);

  char *argv_yes[] = {prog, yes_flag};
  agpm::CliOptions opts_yes = agpm::parse_cli(2, argv_yes);
  REQUIRE(opts_yes.assume_yes);

  char delete_flag[] = "--delete-stray";
  char *argv_delete[] = {prog, yes_flag, delete_flag};
  agpm::CliOptions opts_delete = agpm::parse_cli(3, argv_delete);
  REQUIRE(opts_delete.delete_stray);

  char limit_flag[] = "--pr-limit";
  char limit_val[] = "25";
  char *argv16[] = {prog, limit_flag, limit_val};
  agpm::CliOptions opts16 = agpm::parse_cli(3, argv16);
  REQUIRE(opts16.pr_limit == 25);

  char *argv17[] = {prog};
  agpm::CliOptions opts17 = agpm::parse_cli(1, argv17);
  REQUIRE(opts17.pr_limit == 50);

  char since_flag[] = "--pr-since";
  char since_val[] = "2h";
  char *argv18[] = {prog, since_flag, since_val};
  agpm::CliOptions opts18 = agpm::parse_cli(3, argv18);
  REQUIRE(opts18.pr_since == std::chrono::hours(2));

  char *argv19[] = {prog};
  agpm::CliOptions opts19 = agpm::parse_cli(1, argv19);
  REQUIRE(opts19.pr_since == std::chrono::seconds(0));

  char purge_flag[] = "--purge-prefix";
  char purge_val[] = "tmp/";
  char *argv20[] = {prog, yes_flag, purge_flag, purge_val};
  agpm::CliOptions opts20 = agpm::parse_cli(4, argv20);
  REQUIRE(opts20.purge_prefix == "tmp/");

  char auto_merge_flag[] = "--auto-merge";
  char *argv21[] = {prog, yes_flag, auto_merge_flag};
  agpm::CliOptions opts21 = agpm::parse_cli(3, argv21);
  REQUIRE(opts21.auto_merge);

  char purge_only_flag[] = "--purge-only";
  char *argv21b[] = {prog, yes_flag, purge_only_flag};
  agpm::CliOptions opts21b = agpm::parse_cli(3, argv21b);
  REQUIRE(opts21b.purge_only);

  char *argv22[] = {prog};
  agpm::CliOptions opts22 = agpm::parse_cli(1, argv22);
  REQUIRE(!opts22.auto_merge);
  REQUIRE(!opts22.purge_only);

  char sort_flag[] = "--sort";
  char sort_alpha[] = "alpha";
  char *argv_sort_alpha[] = {prog, sort_flag, sort_alpha};
  agpm::CliOptions sort_opts_alpha = agpm::parse_cli(3, argv_sort_alpha);
  REQUIRE(sort_opts_alpha.sort == "alpha");

  char sort_reverse[] = "reverse";
  char *argv_sort_reverse[] = {prog, sort_flag, sort_reverse};
  agpm::CliOptions sort_opts_reverse = agpm::parse_cli(3, argv_sort_reverse);
  REQUIRE(sort_opts_reverse.sort == "reverse");

  char sort_alphanum[] = "alphanum";
  char *argv_sort_alphanum[] = {prog, sort_flag, sort_alphanum};
  agpm::CliOptions sort_opts_alphanum = agpm::parse_cli(3, argv_sort_alphanum);
  REQUIRE(sort_opts_alphanum.sort == "alphanum");

  char sort_rev_alpha[] = "reverse-alphanum";
  char *argv_sort_rev_alpha[] = {prog, sort_flag, sort_rev_alpha};
  agpm::CliOptions sort_opts_rev_alpha =
      agpm::parse_cli(3, argv_sort_rev_alpha);
  REQUIRE(sort_opts_rev_alpha.sort == "reverse-alphanum");

  char dl_flag[] = "--download-limit";
  char dl_val[] = "1000";
  char ul_flag[] = "--upload-limit";
  char ul_val[] = "2000";
  char *argv_dl[] = {prog, dl_flag, dl_val, ul_flag, ul_val};
  agpm::CliOptions opts_dl = agpm::parse_cli(5, argv_dl);
  REQUIRE(opts_dl.download_limit == 1000);
  REQUIRE(opts_dl.upload_limit == 2000);

  char max_dl_flag[] = "--max-download";
  char max_dl_val[] = "5000";
  char max_ul_flag[] = "--max-upload";
  char max_ul_val[] = "6000";
  char *argv_max[] = {prog, max_dl_flag, max_dl_val, max_ul_flag, max_ul_val};
  agpm::CliOptions opts_max = agpm::parse_cli(5, argv_max);
  REQUIRE(opts_max.max_download == 5000);
  REQUIRE(opts_max.max_upload == 6000);

  // Single-call testing flags
  char single_prs_flag[] = "--single-open-prs";
  char owner_repo_val[] = "me/repo";
  char *argv_single_prs[] = {prog, single_prs_flag, owner_repo_val};
  agpm::CliOptions opts_single_prs = agpm::parse_cli(3, argv_single_prs);
  REQUIRE(opts_single_prs.single_open_prs_repo == std::string("me/repo"));

  char single_br_flag[] = "--single-branches";
  char owner_repo_val2[] = "octo/repo";
  char *argv_single_br[] = {prog, single_br_flag, owner_repo_val2};
  agpm::CliOptions opts_single_br = agpm::parse_cli(3, argv_single_br);
  REQUIRE(opts_single_br.single_branches_repo == std::string("octo/repo"));

  char export_csv_flag[] = "--export-csv";
  char csv_path[] = "out.csv";
  char export_json_flag[] = "--export-json";
  char json_path[] = "out.json";
  char *argv_export[] = {prog, export_csv_flag, csv_path, export_json_flag,
                         json_path};
  agpm::CliOptions opts_export = agpm::parse_cli(5, argv_export);
  REQUIRE(opts_export.export_csv == "out.csv");
  REQUIRE(opts_export.export_json == "out.json");

  char http_proxy_flag[] = "--http-proxy";
  char http_proxy_val[] = "http://proxy";
  char https_proxy_flag[] = "--https-proxy";
  char https_proxy_val[] = "http://secureproxy";
  char *argv_proxy[] = {prog, http_proxy_flag, http_proxy_val, https_proxy_flag,
                        https_proxy_val};
  agpm::CliOptions opts_proxy = agpm::parse_cli(5, argv_proxy);
  REQUIRE(opts_proxy.http_proxy == "http://proxy");
  REQUIRE(opts_proxy.https_proxy == "http://secureproxy");

  char workers_flag[] = "--workers";
  char workers_val[] = "4";
  char *argv_workers[] = {prog, workers_flag, workers_val};
  agpm::CliOptions opts_workers = agpm::parse_cli(3, argv_workers);
  REQUIRE(opts_workers.workers == 4);

  char workers_zero[] = "0";
  char *argv_workers_zero[] = {prog, workers_flag, workers_zero};
  agpm::CliOptions opts_workers_zero = agpm::parse_cli(3, argv_workers_zero);
  REQUIRE(opts_workers_zero.workers == 0);

  {
    char workers_neg[] = "-1";
    char *argv_workers_neg[] = {prog, workers_flag, workers_neg};
    bool threw = false;
    try {
      agpm::parse_cli(3, argv_workers_neg);
    } catch (const std::exception &) {
      threw = true;
    }
    REQUIRE(threw);
  }

  {
    char bad[] = "--unknown";
    char *argv_bad[] = {prog, bad};
    bool threw = false;
    try {
      agpm::parse_cli(2, argv_bad);
    } catch (const std::exception &) {
      threw = true;
    }
    REQUIRE(threw);
  }

  {
    std::istringstream input("n\n");
    auto *cinbuf = std::cin.rdbuf();
    std::cin.rdbuf(input.rdbuf());
    char auto_merge_flag2[] = "--auto-merge";
    char *argv_cancel[] = {prog, auto_merge_flag2};
    bool threw = false;
    try {
      agpm::parse_cli(2, argv_cancel);
    } catch (const std::exception &) {
      threw = true;
    }
    std::cin.rdbuf(cinbuf);
    REQUIRE(threw);
  }
}
