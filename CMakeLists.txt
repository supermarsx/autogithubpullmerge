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

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optionally fetch/refresh dependencies via vcpkg manifest during configure.
# This runs `vcpkg install` if enabled and vcpkg is available.
option(AGPM_FETCH_DEPS "Run vcpkg install to fetch dependencies" OFF)
if(AGPM_FETCH_DEPS)
  if(DEFINED VCPKG_ROOT)
    if(WIN32)
      set(_VCPKG_EXE "${VCPKG_ROOT}/vcpkg.exe")
    else()
      set(_VCPKG_EXE "${VCPKG_ROOT}/vcpkg")
    endif()
    if(EXISTS "${_VCPKG_EXE}")
      # Determine triplet preference
      set(_TRIPLET "$ENV{VCPKG_DEFAULT_TRIPLET}")
      if(NOT _TRIPLET AND DEFINED VCPKG_TARGET_TRIPLET)
        set(_TRIPLET "${VCPKG_TARGET_TRIPLET}")
      endif()
      if(NOT _TRIPLET)
        if(WIN32)
          set(_TRIPLET "x64-windows-static")
        elseif(APPLE)
          set(_TRIPLET "x64-osx")
        else()
          set(_TRIPLET "x64-linux")
        endif()
      endif()
      message(STATUS "Fetching dependencies via vcpkg (triplet=${_TRIPLET})")
      execute_process(
        COMMAND "${_VCPKG_EXE}" install --triplet "${_TRIPLET}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE _VCPKG_RES
      )
      if(NOT _VCPKG_RES EQUAL 0)
        message(FATAL_ERROR "vcpkg install failed with code ${_VCPKG_RES}")
      endif()
    else()
      message(WARNING "AGPM_FETCH_DEPS=ON but vcpkg not found under VCPKG_ROOT; skipping fetch")
    endif()
  else()
    message(WARNING "AGPM_FETCH_DEPS=ON but VCPKG_ROOT is not set; skipping fetch")
  endif()
endif()

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

include(CTest)

find_package(spdlog REQUIRED)

add_subdirectory(src)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
