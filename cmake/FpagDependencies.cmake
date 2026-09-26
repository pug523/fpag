# Copyright 2026 pugur
# This source code is licensed under the Apache License, Version 2.0
# which can be found in the LICENSE file.

# Third party dependencies.
#
# fmt and xxhash appear in fpag's public headers, so they are part of the
# installed interface and a consumer has to be able to resolve them. Every other
# dependency is either private or test only.
#
# Each dependency follows the same shape: use the one already installed on the
# machine when there is one, otherwise build it from a pinned tag. A dependency
# that this project had to build itself is added to the fpagTargets export set,
# together with its headers, so that an installed libfpag.a stays linkable. A
# dependency that was found instead is resolved by find_dependency() in the
# generated package config.

include_guard(GLOBAL)

include(FetchContent)
find_package(PkgConfig QUIET)

set(FPAG_FMT_VERSION
    "12.2.0"
    CACHE STRING "fmt version to build when no fmt is installed")
set(FPAG_XXHASH_VERSION
    "0.8.3"
    CACHE STRING "xxhash version to build when no xxhash is installed")
set(FPAG_CATCH2_VERSION
    "3.15.2"
    CACHE STRING "Catch2 version used by the unit tests")
set(FPAG_BENCHMARK_VERSION
    "1.9.5"
    CACHE STRING "Google Benchmark version used by the micro benchmarks")

# Remembers a dependency this project had to build itself, so that FpagInstall
# can put it in the export set rather than expecting find_dependency() to
# resolve it on the consumer side. A global property is used because the
# recording happens inside a function.
function(fpag_record_vendored target)
  set_property(GLOBAL APPEND PROPERTY FPAG_VENDORED_TARGETS ${target})
endfunction()

function(fpag_vendored_targets out_var)
  get_property(result GLOBAL PROPERTY FPAG_VENDORED_TARGETS)
  set(${out_var}
      "${result}"
      PARENT_SCOPE)
endfunction()

# Applies the local toolchain choice, such as an alternate standard library, to
# a dependency that is being compiled as part of this build.
function(fpag_propagate_toolchain target)
  if(FPAG_CXX_STDLIB AND NOT WIN32)
    target_compile_options(${target} PRIVATE "-stdlib=${FPAG_CXX_STDLIB}")
    target_link_options(${target} PRIVATE "-stdlib=${FPAG_CXX_STDLIB}")
  endif()
endfunction()

# fmt, public. fpag's own headers include fmt/base.h, fmt/format.h and
# fmt/compile.h, so fmt has to be configured identically here and in every
# consumer, which is why the two settings below are INTERFACE properties.
function(fpag_require_fmt)
  if(TARGET fmt::fmt)
    set(FPAG_FMT_TARGET
        fmt::fmt
        PARENT_SCOPE)
    return()
  endif()

  find_package(fmt QUIET CONFIG)
  if(fmt_FOUND)
    if(FPAG_CXX_STDLIB)
      message(
        WARNING
          "FPAG_CXX_STDLIB is ${FPAG_CXX_STDLIB} but the fmt at ${fmt_DIR} was built already, against whatever standard library its own build used. A mismatch here only shows up at link time. Let this project build fmt, by pointing CMAKE_DISABLE_FIND_PACKAGE_fmt at ON, if the two have to agree.")
    endif()
    set(FPAG_FMT_TARGET
        fmt::fmt
        PARENT_SCOPE)
    return()
  endif()

  FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG "refs/tags/${FPAG_FMT_VERSION}"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)
  set(FMT_MASTER_PROJECT
      OFF
      CACHE BOOL "" FORCE)
  set(FMT_TEST
      OFF
      CACHE BOOL "" FORCE)
  set(FMT_DOC
      OFF
      CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(fmt)
  fpag_propagate_toolchain(fmt)
  fpag_record_vendored(fmt)

  set(FPAG_FMT_TARGET
      fmt::fmt
      PARENT_SCOPE)
endfunction()

# xxhash, public. fpag/hash/xxh3_hasher.h includes xxh3.h directly, and that
# header is a compatibility shim which defines XXH_INLINE_ALL before including
# xxhash.h. The whole of the algorithm is therefore compiled into fpag's
# translation units, and no xxhash library is ever linked. Only the header has
# to stay reachable for consumers.
function(fpag_require_xxhash)
  if(TARGET xxhash::xxhash)
    set(FPAG_XXHASH_TARGET
        xxhash::xxhash
        PARENT_SCOPE)
    return()
  endif()

  find_package(xxhash QUIET CONFIG)
  if(xxhash_FOUND)
    set(FPAG_XXHASH_TARGET
        xxhash::xxhash
        PARENT_SCOPE)
    return()
  endif()

  # xxhash publishes no CMakeLists.txt of its own, so the target is assembled
  # here rather than via FetchContent_MakeAvailable.
  include(FetchContent)
  FetchContent_Declare(
    xxhash_src
    GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
    GIT_TAG "v${FPAG_XXHASH_VERSION}"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)
  FetchContent_MakeAvailable(xxhash_src)

  if(NOT EXISTS "${xxhash_src_SOURCE_DIR}/xxh3.h")
    message(
      FATAL_ERROR
        "The fetched xxhash source at ${xxhash_src_SOURCE_DIR} has no xxh3.h, FPAG_XXHASH_VERSION is probably wrong"
    )
  endif()

  add_library(xxhash INTERFACE)
  add_library(xxhash::xxhash ALIAS xxhash)
  target_include_directories(
    xxhash
    INTERFACE "$<BUILD_INTERFACE:${xxhash_src_SOURCE_DIR}>"
              "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

  fpag_record_vendored(xxhash)
  set(FPAG_XXHASH_SOURCE_DIR
      "${xxhash_src_SOURCE_DIR}"
      PARENT_SCOPE)

  set(FPAG_XXHASH_TARGET
      xxhash::xxhash
      PARENT_SCOPE)
endfunction()

# libunwind, private, Linux only. It is looked up rather than fetched, so that
# an installed fpag built with FPAG_ENABLE_LIBUNWIND does not have to ship the
# unwinder along with it.
#
# The result is consumed as a list of raw flags rather than as a target, because
# a target this project created would then have to appear in the fpagTargets
# export set, while a plain -lunwind works for a consumer that statically links
# libfpag.a without needing to resolve anything through find_package().
#
# libunwind splits its library in two: the generic half lives in libunwind, and
# the per architecture half, which owns the unwinder entry points such as
# _Ux86_64_get_reg, lives in libunwind-<arch>. Neither libunwind.pc nor a plain
# -lunwind names the second one, so it is spelled out here.
function(_fpag_libunwind_arch_library out_var)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    set(arch "x86_64")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(arch "aarch64")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i[3-6]86|x86)$")
    set(arch "i386")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^riscv64$")
    set(arch "riscv64")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^loongarch64$")
    set(arch "loongarch64")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^ppc64")
    set(arch "ppc64")
  else()
    set(arch "generic")
  endif()
  set(${out_var}
      "${arch}"
      PARENT_SCOPE)
endfunction()

function(fpag_require_libunwind)
  _fpag_libunwind_arch_library(fpag_libunwind_arch)

  set(FPAG_LIBUNWIND_LDFLAGS
      "-lunwind;-lunwind-${fpag_libunwind_arch}"
      PARENT_SCOPE)
  set(FPAG_LIBUNWIND_INCLUDE_DIRS
      ""
      PARENT_SCOPE)

  if(PkgConfig_FOUND)
    pkg_check_modules(PC_LIBUNWIND QUIET libunwind)
  endif()
  if(PC_LIBUNWIND_FOUND)
    set(FPAG_LIBUNWIND_INCLUDE_DIRS
        "${PC_LIBUNWIND_INCLUDE_DIRS}"
        PARENT_SCOPE)
    return()
  endif()

  message(
    FATAL_ERROR
      "FPAG_ENABLE_LIBUNWIND is on but libunwind was not found. Install the libunwind development package, for example `apt install libunwind-dev`."
  )
endfunction()

# Catch2, test only. fpag brings its own main() because the process level
# handlers and the profiler have to wrap the Catch2 session, so the library
# variant is used rather than Catch2WithMain.
# Catch2, test only. fpag brings its own main() because the process level
# handlers and the profiler have to wrap the Catch2 session, so the library
# variant is used rather than Catch2WithMain.
#
# Also sets FPAG_CATCH_MODULE_DIR, the directory holding Catch.cmake, which
# ships with Catch2 itself rather than with CMake.
function(fpag_require_catch2)
  if(TARGET Catch2::Catch2)
    set(FPAG_CATCH2_TARGET
        Catch2::Catch2
        PARENT_SCOPE)
    return()
  endif()

  find_package(Catch2 QUIET CONFIG)
  if(Catch2_FOUND)
    set(FPAG_CATCH_MODULE_DIR
        "${Catch2_DIR}/extras"
        PARENT_SCOPE)
    set(FPAG_CATCH2_TARGET
        Catch2::Catch2
        PARENT_SCOPE)
    return()
  endif()

  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG "refs/tags/v${FPAG_CATCH2_VERSION}"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)
  set(CATCH_INSTALL_DOCS
      OFF
      CACHE BOOL "" FORCE)
  set(CATCH_INSTALL_EXTRAS
      OFF
      CACHE BOOL "" FORCE)
  set(CATCH_DEVELOPMENT_BUILD
      OFF
      CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(Catch2)
  fpag_propagate_toolchain(Catch2)

  set(FPAG_CATCH_MODULE_DIR
      "${catch2_SOURCE_DIR}/extras"
      PARENT_SCOPE)
  set(FPAG_CATCH2_TARGET
      Catch2::Catch2
      PARENT_SCOPE)
endfunction()

# Google Benchmark, test only. fpag is built without exceptions, so the parts of
# Benchmark that assume a throwing standard library are switched off.
function(fpag_require_benchmark)
  if(TARGET benchmark::benchmark)
    set(FPAG_BENCHMARK_TARGET
        benchmark::benchmark
        PARENT_SCOPE)
    return()
  endif()

  find_package(benchmark QUIET CONFIG)
  if(benchmark_FOUND)
    set(FPAG_BENCHMARK_TARGET
        benchmark::benchmark
        PARENT_SCOPE)
    return()
  endif()

  foreach(case_opt BENCHMARK_ENABLE_TESTING BENCHMARK_ENABLE_GTEST_TESTS
                 BENCHMARK_ENABLE_INSTALL BENCHMARK_INSTALL_DOCS
                 BENCHMARK_ENABLE_WERROR)
    string(TOLOWER "${case_opt}" lower_opt)
    set(${case_opt}
        OFF
        CACHE BOOL "" FORCE)
    set(${lower_opt}
        OFF
        CACHE BOOL "" FORCE)
  endforeach()
  FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG "refs/tags/v${FPAG_BENCHMARK_VERSION}"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)
  FetchContent_MakeAvailable(benchmark)
  fpag_propagate_toolchain(benchmark)

  set(FPAG_BENCHMARK_TARGET
      benchmark::benchmark
      PARENT_SCOPE)
endfunction()
