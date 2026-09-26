# Copyright 2026 pugur
# This source code is licensed under the Apache License, Version 2.0
# which can be found in the LICENSE file.

# Developer tooling, as build targets. These replace what used to be xmake
# tasks, so that a contributor only ever needs cmake:
#
#   cmake --build <dir> --target format       rewrite the sources in place
#   cmake --build <dir> --target format-check report what clang-format changes
#   cmake --build <dir> --target tidy         apply clang-tidy fixes
#   cmake --build <dir> --target cpplint      report what cpplint finds
#   cmake --build <dir> --target lint         format-check plus cpplint
#   cmake --build <dir> --target license      apply the license header
#
# clang-tidy also runs as a compiler launcher in the tidy preset, which is the
# form that sees exactly the command lines the compiler will use. The `tidy`
# target below is the standalone, per file invocation.

include_guard(GLOBAL)

find_program(FPAG_CLANG_FORMAT NAMES clang-format)
find_program(FPAG_CLANG_TIDY NAMES clang-tidy)
find_program(FPAG_CPPLINT NAMES cpplint)
find_program(FPAG_UV NAMES uv)

# The file list is passed through a script rather than on the command line, see
# cmake/run-clang-format.cmake.
function(fpag_add_format_target name mode)
  if(NOT FPAG_CLANG_FORMAT)
    message(STATUS "clang-format was not found, the ${name} target is unavailable")
    return()
  endif()
  add_custom_target(
    ${name}
    COMMAND "${CMAKE_COMMAND}" "-DFPAG_MODE=${mode}"
            "-DFPAG_CLANG_FORMAT=${FPAG_CLANG_FORMAT}"
            "-DFPAG_SOURCE_DIR=${PROJECT_SOURCE_DIR}" -P
            "${PROJECT_SOURCE_DIR}/cmake/run-clang-format.cmake"
    COMMENT "${name}"
    VERBATIM)
endfunction()

fpag_add_format_target(format fix)
fpag_add_format_target(format-check check)

if(FPAG_CLANG_TIDY)
  add_custom_target(
    tidy
    COMMAND "${CMAKE_COMMAND}" "-DFPAG_MODE=fix"
            "-DFPAG_CLANG_FORMAT=${FPAG_CLANG_FORMAT}"
            "-DFPAG_SOURCE_DIR=${PROJECT_SOURCE_DIR}" -P
            "${PROJECT_SOURCE_DIR}/cmake/run-clang-format.cmake"
    COMMAND "${CMAKE_COMMAND}" "-DFPAG_CLANG_TIDY=${FPAG_CLANG_TIDY}"
            "-DFPAG_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DFPAG_BINARY_DIR=${PROJECT_BINARY_DIR}" -P
            "${PROJECT_SOURCE_DIR}/cmake/run-clang-tidy.cmake"
    COMMENT "tidy"
    VERBATIM)
else()
  message(STATUS "clang-tidy was not found, the tidy target is unavailable")
endif()

if(FPAG_CPPLINT)
  if(FPAG_UV)
    set(cpplint_command "${FPAG_UV}" run cpplint)
  else()
    set(cpplint_command "${FPAG_CPPLINT}")
  endif()
  add_custom_target(
    cpplint
    COMMAND ${cpplint_command} --recursive src include tests benchmarks
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "cpplint"
    VERBATIM)
else()
  message(STATUS "cpplint was not found, the cpplint target is unavailable")
endif()

# The aggregate the CI lint job runs.
add_custom_target(
  lint
  DEPENDS format-check)
if(TARGET cpplint)
  add_dependencies(lint cpplint)
endif()

# Applies the license header to any source file that is missing one. The script
# is idempotent, so running this is safe.
if(FPAG_UV)
  add_custom_target(
    license
    COMMAND "${FPAG_UV}" sync
    COMMAND "${FPAG_UV}" run "${PROJECT_SOURCE_DIR}/scripts/header_license.py"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "license"
    VERBATIM)
endif()
