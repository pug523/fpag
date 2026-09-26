# Copyright 2026 pugur
# This source code is licensed under the Apache License, Version 2.0
# which can be found in the LICENSE file.

# The three build targets: the library itself, the unit tests and the micro
# benchmarks. The source lists are explicit rather than globbed, so that adding
# or removing a file is a visible change in review and a stale build directory
# can never silently drop a translation unit.

include_guard(GLOBAL)

fpag_require_fmt()
fpag_require_xxhash()

if(FPAG_ENABLE_LIBUNWIND)
  fpag_require_libunwind()
endif()

set(FPAG_SOURCES
    src/arg/command.cc
    src/arg/error_formatter.cc
    src/arg/help_formatter.cc
    src/arg/parser.cc
    src/arg/version_formatter.cc
    src/base/numeric.cc
    src/container/spsc_queue.cc
    src/debug/check.cc
    src/debug/dlog.cc
    src/debug/exit_handler.cc
    src/debug/fatal.cc
    src/debug/logger.cc
    src/debug/signal_handler.cc
    src/debug/terminate_handler.cc
    src/debug/profiler/profiler.cc
    src/debug/profiler/profile_section.cc
    src/debug/profiler/time_trace_formatter.cc
    src/debug/stack_trace/capture_stack_addresses.cc
    src/debug/stack_trace/demangle.cc
    src/debug/stack_trace/formatter.cc
    src/debug/stack_trace/module_map.cc
    src/debug/stack_trace/stack_trace.cc
    src/debug/stack_trace/symbolicator.cc
    src/io/file_handle.cc
    src/io/io_util.cc
    src/io/memory_mapped_file.cc
    src/io/temp_dir.cc
    src/io/temp_file.cc
    src/mem/arena.cc
    src/mem/concurrent_arena.cc
    src/mem/page_allocator.cc
    src/str/string_interner.cc
    src/str/string_pool.cc
    src/term/console.cc)

function(fpag_add_library)
  add_library(fpag STATIC ${FPAG_SOURCES})
  add_library(fpag::fpag ALIAS fpag)

  target_compile_features(fpag PUBLIC cxx_std_20)

  set_target_properties(
    fpag
    PROPERTIES VERSION ${PROJECT_VERSION}
               SOVERSION ${PROJECT_VERSION_MAJOR}
               POSITION_INDEPENDENT_CODE ON
               CXX_VISIBILITY_PRESET hidden
               VISIBILITY_INLINES_HIDDEN ON
               EXPORT_NAME fpag)

  # <BUILD_INTERFACE> keeps the source tree out of the installed config, and
  # src/ is on the path because the private headers and the internal includes of
  # the test and benchmark targets both reach into it.
  target_include_directories(
    fpag
    PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE "${PROJECT_SOURCE_DIR}/src")

  target_link_libraries(fpag PUBLIC ${FPAG_FMT_TARGET} ${FPAG_XXHASH_TARGET})

  # fmt's inline definitions are compiled into fpag's own translation units and
  # into every consumer's. If the two disagree about consteval format string
  # checking, the result is an ODR violation that only shows up as a crash, so
  # the setting travels with the target rather than being set locally.
  target_compile_definitions(
    fpag PUBLIC FMT_USE_CONSTEVAL=1 FMT_USE_CONSTEXPR=1
    PRIVATE FPAG_PROJECT_VERSION="${PROJECT_VERSION}")

  if(FPAG_ENABLE_LIBUNWIND)
    target_compile_definitions(fpag PRIVATE FPAG_USE_LIBUNWIND=1)
    target_include_directories(fpag PRIVATE ${FPAG_LIBUNWIND_INCLUDE_DIRS})
    target_link_libraries(fpag PRIVATE ${FPAG_LIBUNWIND_LDFLAGS})
  else()
    target_compile_definitions(fpag PRIVATE FPAG_USE_LIBUNWIND=0)
  endif()

  if(WIN32)
    # dbghelp backs the symbolicator, onecore is the Win32 API set the
    # structured exception helpers live in.
    target_link_libraries(fpag PRIVATE dbghelp onecore)
  endif()

  fpag_apply_options(fpag COVERAGE)
endfunction()

function(fpag_add_tests)
  if(NOT FPAG_BUILD_TESTS)
    return()
  endif()

  fpag_require_catch2()

  add_executable(
    fpag_tests
    tests/arg_test.cc
    tests/arena_test.cc
    tests/async_logger_test.cc
    tests/capture_stack_addresses_test.cc
    tests/command_test.cc
    tests/converter_test.cc
    tests/location_test.cc
    tests/macro_test.cc
    tests/matches_test.cc
    tests/math_util_test.cc
    tests/memory_mapped_file_test.cc
    tests/memory_mapped_stream_writer_test.cc
    tests/page_allocator_test.cc
    tests/parser_test.cc
    tests/profile_scope_test.cc
    tests/profile_section_test.cc
    tests/profiler_test.cc
    tests/result_test.cc
    tests/simple_concurrent_hash_map_test.cc
    tests/sinks_test.cc
    tests/soo_vec_test.cc
    tests/spsc_queue_test.cc
    tests/stack_trace_test.cc
    tests/sync_logger_test.cc
    tests/tagged_union_test.cc
    tests/temp_dir_test.cc
    tests/test_main.cc
    tests/time_trace_formatter_test.cc
    tests/union_test.cc
    tests/vec_test.cc
    tests/xxh3_hasher_test.cc)

  target_link_libraries(fpag_tests PRIVATE fpag::fpag ${FPAG_CATCH2_TARGET})
  target_include_directories(fpag_tests PRIVATE "${PROJECT_SOURCE_DIR}/tests")

  fpag_apply_options(fpag_tests COVERAGE)

  # One ctest entry per TEST_CASE, so a failure names the case and unrelated
  # cases keep running. The session runs from the build directory because the
  # test binary writes a Perfetto trace on the way out and a test run must not
  # be able to dirty the source tree.
  list(APPEND CMAKE_MODULE_PATH "${FPAG_CATCH_MODULE_DIR}")
  include(Catch)
  catch_discover_tests(
    fpag_tests
    PROPERTIES WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
    TEST_PREFIX "fpag.")
endfunction()

function(fpag_add_benchmarks)
  if(NOT FPAG_BUILD_BENCHMARKS)
    return()
  endif()

  fpag_require_benchmark()

  add_executable(
    fpag_benchmarks
    benchmarks/async_logger_bench.cc
    benchmarks/benchmark_main.cc
    benchmarks/simple_concurrent_hash_map_bench.cc
    benchmarks/spsc_queue_bench.cc)

  target_link_libraries(fpag_benchmarks PRIVATE fpag::fpag
                                             ${FPAG_BENCHMARK_TARGET})
  target_include_directories(fpag_benchmarks
                             PRIVATE "${PROJECT_SOURCE_DIR}/benchmarks")

  fpag_apply_options(fpag_benchmarks)
endfunction()

fpag_add_library()
fpag_add_tests()
fpag_add_benchmarks()
