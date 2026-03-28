// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/debug/check.h"

#include <cstdio>
#include <cstring>
#include <string_view>

#include "base/debug/fatal.h"
#include "base/debug/stack_trace/stack_trace.h"
#include "base/debug/string.h"
#include "base/io_util.h"
#include "base/logging/sync_logger.h"
#include "base/numeric.h"

namespace base::internal {

void check_fail_impl(const char* expr,
                     const char* file,
                     i32 line,
                     const char* func,
                     std::string_view msg) {
  SyncLogger& logger = global_logger();
  if (msg.empty()) {
    logger.fatal("Check failed!\nExpected: '{}'\n  at {}:{} ({})", expr, file,
                 line, func);
  } else {
    logger.fatal("Check failed!\nExpected: '{}'\n  at {}:{} ({})\n{}", expr,
                 file, line, func, msg);
  }
  print_stack_trace_from_here();
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
  if (msg.empty()) {
    logger.fatal(
        "Check failed!\nExpected: '{}', Actual: {} vs {}\n  at {}:{} ({})",
        expected, lhs, rhs, file, line, func);

  } else {
    logger.fatal(
        "Check failed!\nExpected: '{}', Actual: {} vs {}\n  at {}:{} ({})\n{}",
        expected, lhs, rhs, file, line, func, msg);
  }
  print_stack_trace_from_here();
  logger.flush();

  fatal_crash_impl();
}

void raw_check_fail_impl(const char* expr,
                         const char* file,
                         i32 line,
                         const char* func,
                         std::string_view msg) {
  write(kStderrFd, "fatal: RAW CHECK FAILED for '",
        const_strlen("fatal: RAW CHECK FAILED for '"));
  write(kStderrFd, expr, std::strlen(expr));
  write(kStderrFd, "'\n", const_strlen("'\n"));

  write(kStderrFd, " at ", const_strlen(" at "));
  write(kStderrFd, file, std::strlen(file));
  write(kStderrFd, ":", 1);

  char line_buf[32];
  std::snprintf(line_buf, sizeof(line_buf), "%d", line);
  write(kStderrFd, line_buf, std::strlen(line_buf));

  write(kStderrFd, " (", const_strlen(" ("));
  write(kStderrFd, func, std::strlen(func));
  write(kStderrFd, ")\n", const_strlen(")\n"));

  if (!msg.empty()) {
    write(kStderrFd, msg.data(), msg.size());
    write(kStderrFd, "\n", const_strlen("\n"));
  }

  fatal_crash_impl();
}

}  // namespace base::internal
