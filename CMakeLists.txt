cmake_minimum_required(VERSION 3.16)

# Detect vcpkg toolchain on Windows if not provided
if(WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE
        "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file" FORCE)
  elseif(EXISTS
         "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file" FORCE)
  else()
    message(
      FATAL_ERROR
        "Vcpkg toolchain file not found. Set VCPKG_ROOT or install vcpkg under the project directory."
    )
  endif()
endif()

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
