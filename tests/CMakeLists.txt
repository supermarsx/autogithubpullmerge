find_package(Catch2 3 REQUIRED)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS *.cpp)

add_executable(agpm_tests ${TEST_SOURCES})
target_link_libraries(agpm_tests PRIVATE autogithubpullmerge_lib Catch2::Catch2WithMain)

add_test(NAME agpm_tests COMMAND agpm_tests)
