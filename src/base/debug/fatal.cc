// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/debug/fatal.h"

#include <format>
#include <iostream>

#include "base/log_prefix.h"
#include "build/build_flag.h"

#if FPAG_BUILDFLAG(COMPILER_MSVC)
#include <intrin.h>
#endif

namespace base::internal {

void fatal_crash_impl() {
#if FPAG_BUILDFLAG(HAS_BUILTIN_TRAP)
  __builtin_trap();
#elif FPAG_BUILDFLAG(COMPILER_MSVC)
  __debugbreak();
#endif

  // fallback
  std::abort();

#if FPAG_BUILDFLAG(COMPILER_CLANG_OR_GCC)
  __builtin_unreachable();
#elif FPAG_BUILDFLAG(COMPILER_MSVC)
  __assume(false);
#endif

  while (true) {}
}

void unreachable_impl(const char* file,
                      i32 line,
                      const char* func,
                      std::string_view msg) {
  std::cerr << std::format("{}UNREACHABLE\n", fatal_prefix());
  if (!msg.empty()) {
    std::cerr << msg << '\n';
  }
  std::cerr << std::format("  at {}:{} ({})\n", file, line, func);
  fatal_crash_impl();
}

}  // namespace base::internal
