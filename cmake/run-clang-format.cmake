# Copyright 2026 pugur
# This source code is licensed under the Apache License, Version 2.0
# which can be found in the LICENSE file.

# Runs clang-format over every source file in the project.
#
# This is a script rather than a bare custom target command because the file
# list is a few hundred entries long, and passing it on the command line runs
# into the length limit of the Windows command interpreter.
#
# Invoked as:
#   cmake -DFPAG_MODE=fix|check -DFPAG_CLANG_FORMAT=... -P run-clang-format.cmake

if(NOT FPAG_SOURCE_DIR OR NOT FPAG_CLANG_FORMAT)
  message(FATAL_ERROR "FPAG_SOURCE_DIR and FPAG_CLANG_FORMAT are required")
endif()

if(FPAG_MODE STREQUAL "check")
  set(mode_options --dry-run --Werror)
else()
  set(mode_options -i)
endif()

file(
  GLOB_RECURSE sources
  "${FPAG_SOURCE_DIR}/src/*.cc"
  "${FPAG_SOURCE_DIR}/src/*.h"
  "${FPAG_SOURCE_DIR}/include/*.h"
  "${FPAG_SOURCE_DIR}/tests/*.cc"
  "${FPAG_SOURCE_DIR}/tests/*.h"
  "${FPAG_SOURCE_DIR}/benchmarks/*.cc"
  "${FPAG_SOURCE_DIR}/benchmarks/*.h")

# third_party/ carries a .clang-format-ignore precisely so that a vendored tree
# is skipped, which is why the glob above stops at the module directories.
list(LENGTH sources source_count)
message(STATUS "clang-format ${FPAG_MODE} over ${source_count} file(s)")

execute_process(
  COMMAND "${FPAG_CLANG_FORMAT}" ${mode_options} --fail-on-incomplete-format
          --sort-includes ${sources}
  WORKING_DIRECTORY "${FPAG_SOURCE_DIR}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message("${output}${error}")
  message(FATAL_ERROR "clang-format found code that needs reformatting")
endif()
