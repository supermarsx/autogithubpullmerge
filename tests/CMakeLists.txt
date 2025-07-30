add_executable(tests test_main.cpp)

target_link_libraries(tests PRIVATE autogithubpullmerge_lib)

add_test(NAME main_test COMMAND tests)
