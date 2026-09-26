# Copyright 2026 pugur
# This source code is licensed under the Apache License, Version 2.0
# which can be found in the LICENSE file.

# Every compile and link flag the project needs, expressed with target
# properties and generator expressions. No global compiler flags are set, so
# including fpag through add_subdirectory() cannot leak configuration into the
# parent project.
#
# Usage:
#   fpag_apply_options(<target>)            library, tests and benchmarks
#   fpag_apply_options(<target> COVERAGE)   also instrument for llvm-cov

include_guard(GLOBAL)

include(CheckCXXCompilerFlag)

# Probes a flag once and caches the answer. MSVC spellings are not probed, they
# are recognized by the compiler id.
function(fpag_probe_flag flag out_var)
  string(MAKE_C_IDENTIFIER "FPAG_ACCEPTS_${flag}" cache_var)
  check_cxx_compiler_flag("${flag}" ${cache_var})
  if(${cache_var})
    set(${out_var}
        "${flag}"
        PARENT_SCOPE)
  else()
    set(${out_var}
        ""
        PARENT_SCOPE)
  endif()
endfunction()

function(_fpag_add_probeable_options target)
  # fpag is built without exceptions and without RTTI everywhere, the tests and
  # the benchmarks included, because they are held to the same contract.
  foreach(flag -fno-exceptions -fno-rtti -fstack-protector-strong)
    fpag_probe_flag("${flag}" accepted)
    if(accepted)
      target_compile_options(${target} PRIVATE
                             "$<$<COMPILE_LANGUAGE:CXX>:${accepted}>")
    endif()
  endforeach()

  if(MSVC)
    target_compile_options(${target} PRIVATE
                           "$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    return()
  endif()

  set(warnings
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wsign-conversion
      -Wnull-dereference
      -Wformat=2
      -Wundef
      -Wnon-virtual-dtor
      -Woverloaded-virtual)
  if(FPAG_WARNINGS_AS_ERRORS)
    list(APPEND warnings -Werror)
  endif()
  target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${warnings}>")

  # Some third party headers, which are compiled as part of fpag's public
  # surface, spell C2y constructs inside their own macros.
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${target} PRIVATE
                           "$<$<COMPILE_LANGUAGE:CXX>:-Wno-c2y-extensions>")
  endif()

  # An alternate standard library is a property of the local toolchain, not of
  # the interface, so a consumer that links an installed libfpag is unaffected.
  if(FPAG_CXX_STDLIB)
    target_compile_options(${target} PRIVATE
                           "$<$<COMPILE_LANGUAGE:CXX>:-stdlib=${FPAG_CXX_STDLIB}>")
    target_link_options(${target} PRIVATE "-stdlib=${FPAG_CXX_STDLIB}")
  endif()

  if(FPAG_ENABLE_SANITIZERS)
    set(sanitizers -fsanitize=address,leak,undefined)
    target_compile_options(${target} PRIVATE
                           "$<$<COMPILE_LANGUAGE:CXX>:${sanitizers};-fno-omit-frame-pointer>")
    target_link_options(${target} PRIVATE ${sanitizers})
  endif()

  if(FPAG_ENABLE_TIME_TRACE)
    target_compile_options(${target} PRIVATE
                           "$<$<COMPILE_LANGUAGE:CXX>:-ftime-trace>")
  endif()

  if(FPAG_ENABLE_XRAY AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(xray -fxray-instrument -fxray-instruction-threshold=200)
    target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${xray}>")
    target_link_options(${target} PRIVATE -fxray-instrument)
  endif()

  if(FPAG_ENABLE_OPT_REPORT AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${target} PRIVATE
                           "$<$<COMPILE_LANGUAGE:CXX>:-fsave-optimization-record>")
  endif()

  if(FPAG_ENABLE_NATIVE AND NOT FPAG_CROSS_COMPILING)
    include(ProcessorCount)
    ProcessorCount(fpag_nproc)
    # -march=native is a pessimization on a single core machine, the query
    # reports one, which is the signal that the build is already pinned.
    if(fpag_nproc GREATER 1)
      target_compile_options(${target} PRIVATE
                             "$<$<COMPILE_LANGUAGE:CXX>:-march=native>")
    endif()
  endif()

  if(FPAG_ENABLE_UNITY_BUILD)
    set_target_properties(${target} PROPERTIES UNITY_BUILD ON
                                              UNITY_BUILD_BATCH_SIZE 12)
  endif()

  if(FPAG_ENABLE_LTO)
    set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
endfunction()

function(_fpag_add_clang_tidy target)
  find_program(FPAG_CLANG_TIDY NAMES clang-tidy)
  if(NOT FPAG_CLANG_TIDY)
    message(WARNING "clang-tidy was not found, FPAG_ENABLE_CLANG_TIDY has no effect")
    return()
  endif()

  # Set as a target property rather than through CMAKE_CXX_CLANG_TIDY, so that
  # a dependency this project fetched is not linted against fpag's rules. The
  # launcher form is used rather than a per file invocation because it sees
  # exactly the command line the compiler will use.
  set_target_properties(
    ${target} PROPERTIES CXX_CLANG_TIDY
                       "${FPAG_CLANG_TIDY};--config-file=${PROJECT_SOURCE_DIR}/.clang-tidy")
endfunction()

# Applies the shared flag set. Debug builds keep frame pointers and a build id
# so stack traces and llvm-cov line tables resolve; release builds optimize hard
# and hide symbols, which is what CMAKE_CXX_FLAGS_RELEASE is already set to.
function(fpag_apply_options target)
  cmake_parse_arguments(ARG "COVERAGE" "" "" ${ARGN})

  _fpag_add_probeable_options(${target})
  if(ARG_COVERAGE)
    # Defined in FpagCoverage.cmake, which owns the profile file naming that has
    # to agree between the compiler flags and the report script.
    fpag_add_coverage_options(${target})
  endif()
  if(FPAG_ENABLE_CLANG_TIDY)
    _fpag_add_clang_tidy(${target})
  endif()

  if(MSVC)
    return()
  endif()

  target_compile_options(${target} PRIVATE
                         "$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Debug>>:-fno-omit-frame-pointer;-g1>")

  if(NOT WIN32)
    # Exports the dynamic symbol table, which the stack unwinder and the
    # symbolicator both need in order to name anything in a debug build.
    target_link_options(
      ${target}
      PRIVATE "$<$<AND:$<LINK_LANGUAGE:CXX>,$<CONFIG:Debug>>:-rdynamic>")
  endif()

  if(UNIX AND NOT APPLE)
    target_link_options(
      ${target}
      PRIVATE "$<$<AND:$<LINK_LANGUAGE:CXX>,$<CONFIG:Debug>>:-Wl,--build-id>")
  endif()
endfunction()
