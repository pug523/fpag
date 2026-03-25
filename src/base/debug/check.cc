// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/debug/check.h"

#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <format>
#include <string_view>

#include "base/debug/fatal.h"
#include "base/logging/sync_logger.h"
#include "base/numeric.h"

namespace base::internal {

namespace {

inline constexpr usize const_strlen(const char* s) {
  usize len = 0;
  while (s[len] != '\0') {
    len++;
  }
  return len;
}

}  // namespace

void check_fail_impl(const char* expr,
                     const char* file,
                     i32 line,
                     const char* func,
                     std::string_view msg) {
  SyncLogger& logger = global_logger();
  logger.fatal("Check failed!\nExpected: '{}'\n{}\n  at {}:{} ({})\n", expr,
               msg, file, line, func);
  logger.flush();
  fatal_crash_impl();
}

void check_op_fail_impl(const char* expected,
                        const std::string_view lhs,
                        const std::string_view rhs,
                        const char* file,
                        i32 line,
                        const char* func,
                        std::string_view msg) {
  SyncLogger& logger = global_logger();
  logger.fatal(
      "Check failed!\nExpected: '{}', Actual: {} vs {}\n{}\n  at {}:{} ({})\n",
      expected, lhs, rhs, msg, file, line, func);
  logger.flush();
  fatal_crash_impl();
}

void raw_check_fail_impl(const char* expr,
                         const char* file,
                         i32 line,
                         const char* func,
                         std::string_view msg) {
  write(2, "fatal: RAW CHECK FAILED for '",
        const_strlen("fatal: RAW CHECK FAILED for '"));
  write(2, expr, std::strlen(expr));
  write(2, "'\n", const_strlen("'\n"));

  if (!msg.empty()) {
    write(2, msg.data(), msg.size());
    write(2, "\n", const_strlen("\n"));
  }

  write(2, " at ", const_strlen(" at "));
  write(2, file, std::strlen(file));
  write(2, ":", 1);

  char line_buf[32];
  std::snprintf(line_buf, sizeof(line_buf), "%d", line);
  write(2, line_buf, std::strlen(line_buf));

  write(2, " (", const_strlen(" ("));
  write(2, func, std::strlen(func));
  write(2, ")\n", const_strlen(")\n"));

  fatal_crash_impl();
}

}  // namespace base::internal
