# Copyright 2026 pugur
# This source code is licensed under the Apache License, Version 2.0
# which can be found in the LICENSE file.

# Invoked by the `coverage` target:
#   cmake -DFPAG_BINARY_DIR=... -DFPAG_SOURCE_DIR=... -DFPAG_TARGET_FILE=...
#         -DFPAG_PROFTOOLS_DIR=... -P coverage.cmake
#
# Kept as a script rather than a chain of add_custom_target commands so that
# globbing the raw profiles, tolerating an empty run and giving a readable
# summary are all easy, and so the same code works on every generator.

foreach(required FPAG_BINARY_DIR FPAG_SOURCE_DIR FPAG_TARGET_FILE FPAG_PROFTOOLS_DIR)
  if(NOT ${required})
    message(FATAL_ERROR "${required} is not set")
  endif()
endforeach()

set(profdata "${FPAG_PROFTOOLS_DIR}/llvm-profdata")
set(cov "${FPAG_PROFTOOLS_DIR}/llvm-cov")
set(raw_dir "${FPAG_BINARY_DIR}/coverage/raw")
set(merged "${FPAG_BINARY_DIR}/coverage/fpag.profdata")
set(report "${FPAG_BINARY_DIR}/coverage/html")

file(GLOB raw_profiles "${raw_dir}/*.profraw")
if(NOT raw_profiles)
  message(
    FATAL_ERROR
      "No .profraw file under ${raw_dir}. Run the instrumented tests first, for example `ctest --test-dir ${FPAG_BINARY_DIR}`."
  )
endif()

list(LENGTH raw_profiles raw_count)
message(STATUS "Merging ${raw_count} raw profile(s) into ${merged}")

execute_process(
  COMMAND "${profdata}" merge -sparse -o "${merged}" ${raw_profiles}
  RESULT_VARIABLE result
  OUTPUT_VARIABLE merge_output
  ERROR_VARIABLE merge_error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "llvm-profdata merge failed:\n${merge_output}${merge_error}")
endif()

file(MAKE_DIRECTORY "${report}")

# The library sources and the public headers are the interesting surface, so
# only those are reported on.
execute_process(
  COMMAND "${cov}" show "${FPAG_TARGET_FILE}" "-instr-profile=${merged}"
          -format=html "-output-dir=${report}" "${FPAG_SOURCE_DIR}/include"
          "${FPAG_SOURCE_DIR}/src"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE show_output
  ERROR_VARIABLE show_error)
if(NOT result EQUAL 0)
  message(
    FATAL_ERROR
      "llvm-cov show failed (${result}):\n${show_output}${show_error}")
endif()

# A short summary in the build log, because an HTML report is not something
# anyone reads there.
execute_process(
  COMMAND "${cov}" report "${FPAG_TARGET_FILE}" "-instr-profile=${merged}"
          "${FPAG_SOURCE_DIR}/include" "${FPAG_SOURCE_DIR}/src"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE summary
  ERROR_VARIABLE summary_error)
if(result EQUAL 0)
  message(STATUS "${summary}")
else()
  message(STATUS "llvm-cov report failed:\n${summary_error}")
endif()

message(STATUS "Coverage report: ${report}/index.html")
