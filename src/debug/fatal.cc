// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/debug/fatal.h"

#include <cstdlib>
#include <string_view>

#include "fmt/compile.h"
#include "fpag/base/numeric.h"
#include "fpag/build/build_config.h"
#include "fpag/debug/check.h"
#include "fpag/debug/logger.h"

#if FPAG_BUILD_FLAG(IS_COMPILER_MSVC)
#include <intrin.h>
#endif

namespace debug::internal {

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
  logger.flush();
  fatal_crash_impl();
}

}  // namespace debug::internal
