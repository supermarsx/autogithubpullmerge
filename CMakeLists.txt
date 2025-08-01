cmake_minimum_required(VERSION 3.16)
project(autogithubpullmerge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if(MSVC)
  add_compile_options(/W4 /O2)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -O2)
endif()

enable_testing()

set(LIBS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs)
if(EXISTS "${LIBS_DIR}/spdlog/CMakeLists.txt")
  add_subdirectory(${LIBS_DIR}/spdlog EXTERNAL_SPDLOG_ROOT)
else()
  find_package(spdlog REQUIRED)
endif()

add_subdirectory(src)
add_subdirectory(tests)
