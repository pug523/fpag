# Copyright 2026 pugur
# This source code is licensed under the Apache License, Version 2.0
# which can be found in the LICENSE file.

# Build options. Every option that changes a compile or link flag lives here so
# that the set of knobs is discoverable in one place, and so that
# FPagInstall can bake the ones a consumer needs to reproduce into the exported
# package config.

include_guard(GLOBAL)

option(FPAG_BUILD_TESTS "Build the unit tests" ${FPAG_IS_TOP_LEVEL})
option(FPAG_BUILD_BENCHMARKS "Build the micro benchmarks" OFF)
option(FPAG_INSTALL "Generate install and package config rules" ${FPAG_IS_TOP_LEVEL})

option(FPAG_ENABLE_SANITIZERS "Enable the address, leak and undefined sanitizers"
       OFF)
option(FPAG_ENABLE_COVERAGE "Instrument the library for llvm-cov reports" OFF)
option(FPAG_ENABLE_CLANG_TIDY
       "Run clang-tidy over fpag's own translation units while building" OFF)
option(FPAG_ENABLE_LIBUNWIND "Capture stack traces through libunwind" OFF)
option(FPAG_ENABLE_LTO "Enable link time optimization" OFF)
option(FPAG_ENABLE_UNITY_BUILD "Merge translation units to shorten link time"
       OFF)
option(FPAG_ENABLE_TIME_TRACE "Emit clang -ftime-trace JSON per translation unit"
       OFF)
option(FPAG_ENABLE_XRAY "Instrument for llvm-xray" OFF)
option(FPAG_ENABLE_OPT_REPORT "Emit optimization records as YAML" OFF)
option(FPAG_ENABLE_NATIVE "Tune code generation for the build machine" OFF)
option(FPAG_WARNINGS_AS_ERRORS "Turn compiler warnings into errors" ON)
option(FPAG_WERROR
       "Deprecated alias of FPAG_WARNINGS_AS_ERRORS, for compatibility"
       OFF)

# Only meaningful with a Clang or GCC toolchain that ships libc++ / libstdc++.
# It is a local toolchain choice, so it is not part of the exported interface.
#
# A dependency that this project builds itself, such as a fetched fmt, is
# compiled with the same setting, so the two always agree. A dependency that is
# found already installed is built with whatever standard library its own build
# used, and asking for a different one here is a link time mismatch rather than
# a compile error, so it is reported explicitly.
set(FPAG_CXX_STDLIB
    ""
    CACHE STRING "Alternate standard library to link against, e.g. libc++")

if(FPAG_WERROR)
  message(DEPRECATION "FPAG_WERROR is deprecated, use FPAG_WARNINGS_AS_ERRORS")
  set(FPAG_WARNINGS_AS_ERRORS
      ON
      CACHE BOOL "" FORCE)
endif()

# The sanitizers and the coverage instrumentation are both implemented in
# llvm, and coverage rewrites the same -ftime-trace free line table that
# llvm-profdata reads, so they are not combined.
if(FPAG_ENABLE_SANITIZERS AND FPAG_ENABLE_COVERAGE)
  message(FATAL_ERROR "FPAG_ENABLE_SANITIZERS and FPAG_ENABLE_COVERAGE are mutually exclusive")
endif()

if(FPAG_ENABLE_LIBUNWIND AND NOT (UNIX AND NOT APPLE))
  message(
    FATAL_ERROR
      "FPAG_ENABLE_LIBUNWIND is only supported on Linux, the platform that ships libunwind"
  )
endif()

if(FPAG_ENABLE_COVERAGE AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "FPAG_ENABLE_COVERAGE requires Clang, it relies on llvm-cov")
endif()
