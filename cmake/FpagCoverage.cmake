# Copyright 2026 pugur
# This source code is licensed under the Apache License, Version 2.0
# which can be found in the LICENSE file.

# Coverage reporting, built on llvm-cov.
#
# The instrumented build writes one raw profile per process, named after the
# process id, because ctest runs the test binary once per TEST_CASE and a fixed
# file name would have each run overwrite the previous one.

include_guard(GLOBAL)

# The profile file name pattern is baked into the instrumented binary, so the
# directory has to exist before anything runs.
set(FPAG_PROFRAW_DIR
    "${PROJECT_BINARY_DIR}/coverage/raw"
    CACHE INTERNAL "Directory holding the raw llvm coverage profiles")

function(fpag_add_coverage_options target)
  if(NOT FPAG_ENABLE_COVERAGE)
    return()
  endif()
  file(MAKE_DIRECTORY "${FPAG_PROFRAW_DIR}")
  set(flags -fprofile-instr-generate
      -fcoverage-mapping)
  target_compile_options(
    ${target} PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${flags}>;-fprofile-instr-generate=${FPAG_PROFRAW_DIR}/%p.profraw"
  )
  target_link_options(${target} PRIVATE -fprofile-instr-generate)
endfunction()

# llvm-profdata and llvm-cov are not necessarily on PATH, so the directory they
# were found in is handed to the script instead of the executables themselves.
function(fpag_add_coverage_target target)
  if(NOT FPAG_ENABLE_COVERAGE)
    return()
  endif()

  find_program(FPAG_LLVM_PROFDATA NAMES llvm-profdata)
  find_program(FPAG_LLVM_COV NAMES llvm-cov)
  if(NOT FPAG_LLVM_PROFDATA OR NOT FPAG_LLVM_COV)
    message(
      WARNING
        "llvm-profdata and llvm-cov were not found, the `coverage` target is unavailable")
    return()
  endif()
  get_filename_component(
    fpag_proftools_dir "${FPAG_LLVM_PROFDATA}" DIRECTORY)

  add_custom_target(
    coverage
    COMMAND
      "${CMAKE_COMMAND}" "-DFPAG_BINARY_DIR=${PROJECT_BINARY_DIR}"
      "-DFPAG_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
      "-DFPAG_TARGET_FILE=$<TARGET_FILE:${target}>"
      "-DFPAG_PROFTOOLS_DIR=${fpag_proftools_dir}" -P
      "${PROJECT_SOURCE_DIR}/cmake/coverage.cmake"
    DEPENDS ${target}
    COMMENT "Generating the llvm-cov HTML report"
    VERBATIM)
endfunction()
