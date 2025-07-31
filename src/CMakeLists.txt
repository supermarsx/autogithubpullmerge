find_package(yaml-cpp REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(spdlog REQUIRED)
find_package(Curses REQUIRED)


add_library(autogithubpullmerge_lib app.cpp cli.cpp config.cpp config_manager.cpp github_client.cpp history.cpp log.cpp tui.cpp)

target_include_directories(autogithubpullmerge_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include ${CURSES_INCLUDE_DIR})

target_link_libraries(autogithubpullmerge_lib PUBLIC yaml-cpp
                                              nlohmann_json::nlohmann_json
                                              CURL::libcurl SQLite::SQLite3
                                              spdlog::spdlog ${CURSES_LIBRARIES})

add_executable(autogithubpullmerge main.cpp)
target_link_libraries(autogithubpullmerge PRIVATE autogithubpullmerge_lib)
