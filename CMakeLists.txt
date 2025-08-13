cmake_minimum_required(VERSION 3.16)

# Detect vcpkg toolchain if not provided
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
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

# Use MSVC vcpkg triplet on Windows when none is specified
if(WIN32 AND NOT DEFINED VCPKG_TARGET_TRIPLET)
  set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING
                                 "Vcpkg target triplet" FORCE)
endif()

project(autogithubpullmerge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Ensure all targets link against the static MSVC runtime when building on
# Windows. This avoids mismatches between /MT and /MD when linking against
# vcpkg packages built with the static triplet.
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Compiler flags
if(MSVC)
  add_compile_options(/W4 /O2)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -O2)
endif()

enable_testing()

find_package(spdlog REQUIRED)

add_subdirectory(src)
add_subdirectory(tests)
