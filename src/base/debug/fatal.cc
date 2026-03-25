// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/debug/fatal.h"

#include <cstdlib>
#include <format>
#include <string_view>

#include "base/logging/sync_logger.h"
#include "base/numeric.h"
#include "build/build_config.h"

#if FPAG_BUILD_FLAG(IS_COMPILER_MSVC)
#include <intrin.h>
#endif

namespace base::internal {

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

  // fallback
  std::abort();

  while (true) {}
}

void unreachable_impl(const char* file,
                      i32 line,
                      const char* func,
                      std::string_view msg) {
  SyncLogger& logger = global_logger();
  logger.fatal("UNREACHABLE\n{}\n  at {}:{} ({})", msg, file, line, func);
  logger.flush();
  fatal_crash_impl();
}

}  // namespace base::internal
