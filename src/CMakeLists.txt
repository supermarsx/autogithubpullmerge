add_library(autogithubpullmerge_lib app.cpp)

target_include_directories(autogithubpullmerge_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_executable(autogithubpullmerge main.cpp)
target_link_libraries(autogithubpullmerge PRIVATE autogithubpullmerge_lib)
