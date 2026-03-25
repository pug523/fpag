// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/debug.h"

#include <format>
#include <iostream>
#include <string>
#include <string_view>

#if LIBP_IS_DEBUG
namespace base::internal {

void dlog_impl(const char* file,
               i32 line,
               const char* func,
               std::string_view fmt,
               std::format_args args) {
  std::string msg = std::vformat(fmt, args);
  std::clog << std::format("{}{}  [on {} ({}:{})]\n", debug_prefix(), msg, func,
                           file, line)
            << std::flush;
}

void dcheck_fail_impl(const char* expr,
                      const char* file,
                      i32 line,
                      const char* func,
                      std::string_view msg) {
  std::cerr << std::format("{}debug check failed: {}\n", fatal_prefix(), expr);
  if (!msg.empty()) {
    std::cerr << "  message: " << msg << '\n';
  }
  std::cerr << std::format("  at {}:{} ({})\n", file, line, func);
  std::abort();
}

}  // namespace base::internal
#endif
