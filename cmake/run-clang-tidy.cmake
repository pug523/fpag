# Copyright 2026 pugur
# This source code is licensed under the Apache License, Version 2.0
# which can be found in the LICENSE file.

# Runs clang-tidy over every source file in the project, applying the fixes it
# can.
#
# A script rather than a bare custom target command, for the same reason as
# run-clang-format.cmake: the file list does not fit on a Windows command line.
#
# This is the per file form. The build also runs clang-tidy as a compiler
# launcher, under FPAG_ENABLE_CLANG_TIDY, which is the form that sees exactly
# the command lines the compiler will use. Use that one to find out whether
# there is anything to fix, and this one to fix it.
#
# Only translation units are passed in. A header has no compilation database
# entry, and passing one as the main file would bypass HeaderFilterRegex and
# pull the whole public header tree into the check.
#
# Invoked as:
#   cmake -DFPAG_CLANG_TIDY=... -DFPAG_SOURCE_DIR=... -DFPAG_BINARY_DIR=...
#         -P run-clang-tidy.cmake

if(NOT FPAG_SOURCE_DIR OR NOT FPAG_BINARY_DIR OR NOT FPAG_CLANG_TIDY)
  message(
    FATAL_ERROR
      "FPAG_SOURCE_DIR, FPAG_BINARY_DIR and FPAG_CLANG_TIDY are required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/FpagSources.cmake")
fpag_translation_units("${FPAG_SOURCE_DIR}" sources)

list(LENGTH sources source_count)
message(STATUS "clang-tidy over ${source_count} translation unit(s)")

execute_process(
  COMMAND "${FPAG_CLANG_TIDY}" --use-color --fix
          "--config-file=${FPAG_SOURCE_DIR}/.clang-tidy" -p "${FPAG_BINARY_DIR}"
          ${sources}
  WORKING_DIRECTORY "${FPAG_SOURCE_DIR}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error)

message("${output}${error}")
if(NOT result EQUAL 0)
  message(
    FATAL_ERROR
      "clang-tidy found something it could not fix. Fix it by hand, or add a NOLINT with a reason.")
endif()
