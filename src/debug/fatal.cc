// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/fatal.h"

#include <csignal>
#include <string_view>

#include "fmt/compile.h"
#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"
#include "fpag/debug/check.h"
#include "fpag/debug/logger.h"
#include "fpag/debug/stack_trace/stack_trace.h"

#if FPAG_BUILD_FLAG(IS_COMPILER_MSVC)
#include <intrin.h>
#endif

namespace debug::internal {

namespace {

// The signal handler reads this after a deliberate trap, so it stays a
// `sig_atomic_t`: the standard's word for a flag a handler may read.
volatile std::sig_atomic_t stack_trace_was_printed = 0;

}  // namespace

void mark_stack_trace_printed() {
  stack_trace_was_printed = 1;
}

bool stack_trace_printed() {
  return stack_trace_was_printed != 0;
}

void fatal_crash_impl() {
#if FPAG_BUILD_FLAG(IS_DEBUG)
#if FPAG_BUILD_FLAG(IS_COMPILER_GCC)
  __builtin_trap();
#elif FPAG_BUILD_FLAG(IS_COMPILER_MSVC)
  __debugbreak();
#endif
#else
#if FPAG_BUILD_FLAG(IS_COMPILER_GCC)
  __builtin_unreachable();
#elif FPAG_BUILD_FLAG(IS_COMPILER_MSVC)
  __assume(false);
#endif
#endif

  while (true) {}
}

void unreachable_impl(const char* file,
                      i32 line,
                      const char* func,
                      std::string_view msg) {
  DebugLogger& logger = debug_logger;
  if (!logger.has_sink()) [[unlikely]] {
    // Without a sink the report has to go to stderr; see check_fail_impl().
    raw_check_fail_impl("FPAG_UNREACHABLE()", file, line, func, msg);
  }
  logger.fatal(FMT_COMPILE("UNREACHABLE\n{}\n  at {}:{} ({})"), msg, file, line,
               func);
  print_stack_trace_from_here();
  mark_stack_trace_printed();
  logger.flush();
  fatal_crash_impl();
}

}  // namespace debug::internal
