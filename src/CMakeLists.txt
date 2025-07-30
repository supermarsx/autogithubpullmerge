find_package(CLI11 CONFIG REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(nlohmann_json REQUIRED)

include(FetchContent)
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v6.1.9
)
FetchContent_MakeAvailable(ftxui)

add_library(autogithubpullmerge_lib app.cpp cli.cpp config.cpp tui.cpp)

target_include_directories(autogithubpullmerge_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)

target_link_libraries(autogithubpullmerge_lib PUBLIC
  CLI11::CLI11
  yaml-cpp
  nlohmann_json::nlohmann_json
  ftxui::screen
  ftxui::dom)

add_executable(autogithubpullmerge main.cpp)
target_link_libraries(autogithubpullmerge PRIVATE autogithubpullmerge_lib)
