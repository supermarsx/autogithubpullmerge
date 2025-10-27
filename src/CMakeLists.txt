find_package(CLI11 CONFIG REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(ZLIB REQUIRED)
set(EXTRA_WIN_LIBS)
if(WIN32)
  find_package(unofficial-pdcurses CONFIG REQUIRED)
  set(CURSES_LIBRARIES unofficial::pdcurses::pdcurses)
  set(CURSES_INCLUDE_DIR "")
  list(APPEND EXTRA_WIN_LIBS ws2_32)
else()
  find_package(Curses REQUIRED)
endif()

add_library(
  autogithubpullmerge_lib
  app.cpp
  cli.cpp
  pat.cpp
  config.cpp
  config_manager.cpp
  demo_tui.cpp
  github_client.cpp
  mcp_server.cpp
  history.cpp
  hook.cpp
  log.cpp
  rule_engine.cpp
  tui.cpp
  poller.cpp
  github_poller.cpp
  notification.cpp
  repo_discovery.cpp
  token_loader.cpp
  util/duration.cpp)

target_include_directories(
  autogithubpullmerge_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include
                                 ${CMAKE_BINARY_DIR}/generated
                                 ${CURSES_INCLUDE_DIR})

target_link_libraries(
  autogithubpullmerge_lib
  PUBLIC CLI11::CLI11
         yaml-cpp::yaml-cpp
         nlohmann_json::nlohmann_json
         CURL::libcurl
         SQLite::SQLite3
         spdlog::spdlog
         ZLIB::ZLIB
         ${EXTRA_WIN_LIBS}
         ${CURSES_LIBRARIES})


if(WIN32)
  target_compile_definitions(autogithubpullmerge_lib
                             PRIVATE YAML_CPP_STATIC_DEFINE)
endif()
add_executable(autogithubpullmerge main.cpp)
# Ensure the executable can include generated headers directly if needed.
target_include_directories(autogithubpullmerge PRIVATE
                           ${CMAKE_BINARY_DIR}/generated)
#
# Explicitly link external libraries on the final executable.  While
# autogithubpullmerge_lib already links against yaml-cpp and libcurl, transitive
# dependencies are not always propagated reliably on some Windows builds.
# Linking the required libraries here ensures the linker can resolve YAML and
# libcurl symbols when building on Windows.
target_link_libraries(
  autogithubpullmerge PRIVATE autogithubpullmerge_lib yaml-cpp::yaml-cpp
                              CURL::libcurl ZLIB::ZLIB)
