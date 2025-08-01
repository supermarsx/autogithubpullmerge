set(LIBS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libs)

if(EXISTS "${LIBS_DIR}/CLI11/CMakeLists.txt")
  add_subdirectory(${LIBS_DIR}/CLI11 EXTERNAL_CLI11)
else()
  find_package(CLI11 CONFIG REQUIRED)
endif()

if(EXISTS "${LIBS_DIR}/yaml-cpp/CMakeLists.txt")
  add_subdirectory(${LIBS_DIR}/yaml-cpp EXTERNAL_YAMLCPP)
else()
  find_package(yaml-cpp REQUIRED)
endif()

if(EXISTS "${LIBS_DIR}/json/CMakeLists.txt")
  add_subdirectory(${LIBS_DIR}/json EXTERNAL_JSON)
else()
  find_package(nlohmann_json REQUIRED)
endif()

if(EXISTS "${LIBS_DIR}/curl/CMakeLists.txt")
  add_subdirectory(${LIBS_DIR}/curl EXTERNAL_CURL)
else()
  find_package(CURL REQUIRED)
endif()
find_package(SQLite3 REQUIRED)
find_package(Curses REQUIRED)

add_library(autogithubpullmerge_lib
            app.cpp
            cli.cpp
            config.cpp
            config_manager.cpp
            github_client.cpp
            history.cpp
            log.cpp
            tui.cpp
            poller.cpp)

target_include_directories(autogithubpullmerge_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include ${CURSES_INCLUDE_DIR})

target_link_libraries(autogithubpullmerge_lib PUBLIC CLI11::CLI11 yaml-cpp
                                              nlohmann_json::nlohmann_json
                                              CURL::libcurl SQLite::SQLite3
                                              spdlog::spdlog ${CURSES_LIBRARIES})

if(WIN32)
  target_compile_definitions(autogithubpullmerge_lib PRIVATE YAML_CPP_STATIC_DEFINE)
endif()
add_executable(autogithubpullmerge main.cpp)
target_link_libraries(autogithubpullmerge PRIVATE autogithubpullmerge_lib)
