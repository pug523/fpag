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

  if(WIN32)
    # clang-cl and clang++ on Windows both honour the MSVC CRT's deprecation
    # attributes, and both need this to compile _open, fopen and friends. It has
    # to arrive as a definition, before any header, which is why it cannot live
    # inside a source file. MSVC is the wrong test here: it is false for the
    # clang toolchain that CI builds with.
    #
    # NOMINMAX is here for the same reason and in the same place. The Windows
    # headers define min and max as function-like macros, and any translation
    # unit that pulls one of them in before fpag/base/limits.h turns
    # std::numeric_limits<i8>::min() into a macro invocation with too few
    # arguments. It has to be defined for the tests and benchmarks too, not just
    # the library, because they include the same public headers. That is what
    # this function is for: every target in the project goes through it.
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS
                                                    NOMINMAX)
  endif()

  if(MSVC)
    # The rest of the flag set is spelled for a GCC or clang driver. cl.exe
    # wants /W4 and the rest, which is a separate conversation this project is
    # not having yet, so only the encoding flag is set for it.
    target_compile_options(${target} PRIVATE
                           "$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")
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

  # The rest is ELF and Mach-O specific. On Windows the stack trace path goes
  # through dbghelp rather than a frame walk, so frame pointers buy nothing and
  # -rdynamic and --build-id are not even spelled the same way.
  if(NOT UNIX)
    return()
  endif()

  target_compile_options(${target} PRIVATE
                         "$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Debug>>:-fno-omit-frame-pointer;-g1>")

  # Exports the dynamic symbol table, which the stack unwinder and the
  # symbolicator both need in order to name anything in a debug build.
  target_link_options(
    ${target}
    PRIVATE "$<$<AND:$<LINK_LANGUAGE:CXX>,$<CONFIG:Debug>>:-rdynamic>")

  if(NOT APPLE)
    target_link_options(
      ${target}
      PRIVATE "$<$<AND:$<LINK_LANGUAGE:CXX>,$<CONFIG:Debug>>:-Wl,--build-id>")
  endif()
endfunction()
