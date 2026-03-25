// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/debug/fatal.h"

#include <cstdlib>
#include <format>
#include <iostream>
#include <string_view>

#include "base/logging/log_level.h"
#include "base/numeric.h"
#include "build/build_config.h"

#if FPAG_BUILD_FLAG(IS_COMPILER_MSVC)
#include <intrin.h>
#endif

namespace base::internal {

void fatal_crash_impl() {
#if FPAG_BUILD_FLAG(IS_COMPILER_MSVC)
  __builtin_trap();
#elif FPAG_BUILD_FLAG(IS_COMPILER_MSVC)
  __debugbreak();
#endif

  // fallback
  std::abort();

#if FPAG_BUILD_FLAG(IS_COMPILER_MSVC)
  __builtin_unreachable();
#elif FPAG_BUILD_FLAG(IS_COMPILER_MSVC)
  __assume(false);
#endif

  while (true) {}
}

void unreachable_impl(const char* file,
                      i32 line,
                      const char* func,
                      std::string_view msg) {
  std::cerr << std::format("{}UNREACHABLE\n",
                           log_prefix(LogLevel::Fatal, false));
  if (!msg.empty()) {
    std::cerr << msg << '\n';
  }
  std::cerr << std::format("  at {}:{} ({})\n", file, line, func);
  fatal_crash_impl();
}

}  // namespace base::internal
