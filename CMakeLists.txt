# Copyright 2026 pugur
# This source code is licensed under the Apache License, Version 2.0
# which can be found in the LICENSE file.

cmake_minimum_required(VERSION 3.28)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(FPAG_IS_TOP_LEVEL ON)
else()
  set(FPAG_IS_TOP_LEVEL OFF)
endif()

project(
  fpag
  VERSION 0.1.0
  DESCRIPTION "Low level C++20 building blocks: arenas, logging, CLI parsing"
  HOMEPAGE_URL "https://github.com/pug523/fpag"
  LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(GNUInstallDirs)

# clangd and clang-tidy are pointed at this file by the .clangd
# CompilationDatabase setting, so it has to sit at the top of the source tree
# rather than inside whichever build directory was configured last.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Everything that is not provided by the surrounding toolchain is fetched, so a
# bare checkout configures on a machine with nothing but a compiler.
set(FETCHCONTENT_FULLY_DISCONNECTED
    OFF
    CACHE BOOL "Do not re-check fetched dependencies against their remotes")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE
      RelWithDebInfo
      CACHE STRING "Build type" FORCE)
  set_property(
    CACHE CMAKE_BUILD_TYPE
    PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)

# Release builds hide symbols, which is what a library wants, but the tests and
# the benchmarks are executables that people read backtraces of.
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")

include(FpagOptions)
include(FpagCompilerOptions)
include(FpagDependencies)
# FpagCoverage defines fpag_add_coverage_options, which fpag_apply_options calls,
# so it has to be defined before the targets are created.
include(FpagCoverage)

# enable_testing() only has an effect from the top level, and Catch's test
# discovery expects CTest to have been set up first.
if(FPAG_BUILD_TESTS)
  include(CTest)
endif()

include(FpagTargets)
include(FpagTools)

if(FPAG_BUILD_TESTS AND FPAG_ENABLE_COVERAGE)
  fpag_add_coverage_target(fpag_tests)
endif()

if(FPAG_IS_TOP_LEVEL)
  include(FpagInstall)
endif()
