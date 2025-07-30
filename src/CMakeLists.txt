find_package(CLI11 CONFIG REQUIRED)

add_library(autogithubpullmerge_lib app.cpp cli.cpp)

target_include_directories(autogithubpullmerge_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)

target_link_libraries(autogithubpullmerge_lib PUBLIC CLI11::CLI11)

add_executable(autogithubpullmerge main.cpp)
target_link_libraries(autogithubpullmerge PRIVATE autogithubpullmerge_lib)
