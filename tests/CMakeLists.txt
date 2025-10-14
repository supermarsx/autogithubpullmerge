find_package(Catch2 3 REQUIRED)

# Automatically include all test sources
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS *.cpp)

add_executable(agpm_tests ${TEST_SOURCES})
target_link_libraries(agpm_tests PRIVATE autogithubpullmerge_lib Catch2::Catch2WithMain)

# Define two ctest entries: core tests (exclude [tui]) and tui tests (only [tui])
add_test(NAME agpm_tests_core COMMAND ${CMAKE_SOURCE_DIR}/scripts/run_catch_core.sh ${CMAKE_CURRENT_BINARY_DIR}/agpm_tests)
add_test(NAME agpm_tests_tui COMMAND agpm_tests [tui])
add_test(NAME agpm_tests_cli COMMAND agpm_tests [cli])

# CLI tests are known to be sensitive to stdlib ABI differences on some
# environments; keep them separate and disabled by default.
set_tests_properties(agpm_tests_cli PROPERTIES DISABLED TRUE)

# By default, the TUI tests may require a real TTY. Leave them enabled; they
# also contain runtime guards that skip when no TTY is present.
