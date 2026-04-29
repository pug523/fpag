// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/base/debug/check.h"

#include <cstring>
#include <string_view>

#include "fmt/base.h"
#include "fmt/compile.h"
#include "fpag/base/debug/fatal.h"
#include "fpag/base/debug/logger.h"
#include "fpag/base/debug/stack_trace/stack_trace.h"
#include "fpag/base/debug/string.h"
#include "fpag/base/io_util.h"
#include "fpag/base/numeric.h"

namespace base::internal {

void check_fail_impl(const char* expr,
                     const char* file,
                     i32 line,
                     const char* func,
                     std::string_view msg) {
  DebugLogger& logger = debug_logger();
  if (msg.empty()) {
    logger.fatal(FMT_COMPILE("Check failed!\nExpected: '{}'\n  at {}:{} ({})"),
                 expr, file, line, func);
  } else {
    logger.fatal(
        FMT_COMPILE("Check failed!\nExpected: '{}'\n  at {}:{} ({})\n{}"), expr,
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
  DebugLogger& logger = debug_logger();
  if (msg.empty()) {
    logger.fatal(
        FMT_COMPILE(
            "Check failed!\nExpected: '{}', Actual: {} vs {}\n  at {}:{} ({})"),
        expected, lhs, rhs, file, line, func);

  } else {
    logger.fatal(FMT_COMPILE("Check failed!\nExpected: '{}', Actual: {} vs "
                             "{}\n  at {}:{} ({})\n{}"),
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
  constexpr const char* kHeaderPrefix = "fatal: RAW CHECK FAILED for '";
  constexpr const char* kHeaderSuffix = "'\n";
  constexpr const char* kAt = " at ";
  constexpr const char* kColon = " : ";
  constexpr usize kLineBufSize = 64;
  constexpr const char* kFuncPrefix = " (";
  constexpr const char* kFuncSuffix = ")\n";
  constexpr const char* kNewline = "\n";

  ::base::write(kStderrFd, kHeaderPrefix, const_strlen(kHeaderPrefix));
  ::base::write(kStderrFd, expr, std::strlen(expr));
  ::base::write(kStderrFd, kHeaderSuffix, const_strlen(kHeaderSuffix));

  ::base::write(kStderrFd, kAt, const_strlen(kAt));
  ::base::write(kStderrFd, file, std::strlen(file));
  ::base::write(kStderrFd, kColon, const_strlen(kColon));

  char line_buf[kLineBufSize];
  auto result = fmt::format_to_n(line_buf, sizeof(line_buf), "{}", line);
  ::base::write(kStderrFd, line_buf, result.size);

  ::base::write(kStderrFd, kFuncPrefix, const_strlen(kFuncPrefix));
  ::base::write(kStderrFd, func, std::strlen(func));
  ::base::write(kStderrFd, kFuncSuffix, const_strlen(kFuncSuffix));

  if (!msg.empty()) {
    ::base::write(kStderrFd, msg.data(), msg.size());
    ::base::write(kStderrFd, kNewline, const_strlen(kNewline));
  }

  fatal_crash_impl();
}

}  // namespace base::internal
