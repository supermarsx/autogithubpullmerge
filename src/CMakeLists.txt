find_package(CLI11 CONFIG REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Curses REQUIRED)

add_library(
  autogithubpullmerge_lib
  app.cpp
  cli.cpp
  config.cpp
  config_manager.cpp
  github_client.cpp
  history.cpp
  log.cpp
  tui.cpp
  poller.cpp
  github_poller.cpp)

target_include_directories(
  autogithubpullmerge_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include
                                 ${CURSES_INCLUDE_DIR})

target_link_libraries(
  autogithubpullmerge_lib
  PUBLIC CLI11::CLI11
         yaml-cpp::yaml-cpp
         nlohmann_json::nlohmann_json
         CURL::libcurl
         SQLite::SQLite3
         spdlog::spdlog
         ${CURSES_LIBRARIES})

if(WIN32)
  target_compile_definitions(autogithubpullmerge_lib
                             PRIVATE YAML_CPP_STATIC_DEFINE)
endif()
add_executable(autogithubpullmerge main.cpp)
#
# Explicitly link external libraries on the final executable.  While
# autogithubpullmerge_lib already links against yaml-cpp and libcurl, transitive
# dependencies are not always propagated reliably on some Windows builds.
# Linking the required libraries here ensures the linker can resolve YAML and
# libcurl symbols when building on Windows.
target_link_libraries(autogithubpullmerge PRIVATE autogithubpullmerge_lib
                                                  yaml-cpp::yaml-cpp CURL::libcurl)
