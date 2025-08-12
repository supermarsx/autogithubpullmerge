find_package(CLI11 CONFIG REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Curses REQUIRED)
find_package(spdlog REQUIRED)

set(AGPM_TEST_LIBS
    autogithubpullmerge_lib
    CLI11::CLI11
    yaml-cpp::yaml-cpp
    nlohmann_json::nlohmann_json
    CURL::libcurl
    SQLite::SQLite3
    spdlog::spdlog
    ${CURSES_LIBRARIES})

function(add_agpm_test target)
  add_executable(${target} ${ARGN})
  target_link_libraries(${target} PRIVATE ${AGPM_TEST_LIBS})
  string(REGEX REPLACE "^test_" "" base "${target}")
  add_test(NAME "${base}_test" COMMAND ${target})
endfunction()

add_agpm_test(test_main test_main.cpp)
add_agpm_test(test_cli test_cli.cpp)
add_agpm_test(test_config test_config.cpp)
add_agpm_test(test_config_manager test_config_manager.cpp)
add_agpm_test(test_github_client test_github_client.cpp)
add_agpm_test(test_github_client_behavior test_github_client_behavior.cpp)
add_agpm_test(test_history test_history.cpp)
add_agpm_test(test_tui test_tui.cpp)
add_agpm_test(test_github_filter test_github_filter.cpp)
add_agpm_test(test_poller test_poller.cpp)
add_agpm_test(test_pr_since test_pr_since.cpp)
add_agpm_test(test_cli_tokens test_cli_tokens.cpp)
add_agpm_test(test_config_loading test_config_loading.cpp)
add_agpm_test(test_github_client_headers test_github_client_headers.cpp)
add_agpm_test(test_github_client_delay test_github_client_delay.cpp)
add_agpm_test(test_github_client_accept test_github_client_accept.cpp)
add_agpm_test(test_history_merge test_history_merge.cpp)
add_agpm_test(test_github_poller test_github_poller.cpp)
add_agpm_test(test_branch_cleanup test_branch_cleanup.cpp)
add_agpm_test(test_auto_merge test_auto_merge.cpp)
